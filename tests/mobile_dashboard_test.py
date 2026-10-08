"""Headless mobile browser regression against a local fixture, never the car.

Run test.ps1 first, then: python tests/mobile_dashboard_test.py
Optional test dependency: pip install playwright; playwright install chromium
"""
import asyncio
import json
from pathlib import Path
from urllib.parse import parse_qs, urlparse
from playwright.async_api import async_playwright

root = Path(__file__).resolve().parents[1]
source = root/'firmware/CarReady'
html = (source/'dashboard.html').read_text(encoding='utf-8').replace(
    '/*__TOUCH_DRIVE__*/', (source/'TouchDrive.js').read_text(encoding='utf-8')).replace(
    '/*__NETWORK_SETTINGS__*/', (source/'NetworkSettings.js').read_text(encoding='utf-8')).replace('__TOKEN__','test-token')
patterns = json.loads((root/'build/matrix-patterns.json').read_text())
results = []

async def run():
    state = dict(matrix_head_auto=True,matrix_head_target=90,matrix_head_source='idle',home_ssid='Classroom',saved_ssid='Classroom',connected_ssid='Classroom',home_settings_source='saved',wifi_last_result='joined',wifi_join_state='idle',dashboard_url='http://192.168.0.202/',home_url='http://192.168.0.202/',wifi_signal_dbm=-47,network_mode='home',ip='192.168.0.202',hotspot_ssid='Freenove-Rover',firmware='CarReady-2.11',module='matrix',armed=False,moving=False,
        mode='idle',autonomous=False,front_cm=None,distance_cm=None,battery_volts=8,
        battery_percent=77,battery_state='Good',notice='Stopped and disarmed',
        led_effect='auto',sound_alerts=True,brightness=12,sound_volume=60,tone_hz=2000,
        roam_action='Stopped',remaining_s=0,radar=[],escape_turns=0,head_angle=90,
        scan_left_angle=60,range_quality='Matrix fitted',head_cm=None,
        light_delta=[0,0],light_calibrated=False,light_threshold=3,light_action='Stopped',
        light=[28,21],light_baseline=[28,21],line_black=0,line_action='Stopped',
        line=[1,0,1],ir_raw=0,wheels=[0]*4,front_guard=False,matrix_face='auto',
        matrix_active='eyes',matrix_brightness=4,matrix_rotation=270,matrix_rows=patterns['eyes'])
    log = []
    delay_drive = False
    drive_pending = asyncio.Event()
    async def route(request):
        nonlocal delay_drive
        path = urlparse(request.request.url)
        if path.path == '/':
            await request.fulfill(body=html,content_type='text/html')
            return
        if path.path == '/api/status':
            await request.fulfill(json=state)
            return
        if path.path == '/api/stop':
            log.append(('stop',{}))
            state.update(armed=False,moving=False,mode='idle',wheels=[0]*4)
            await request.fulfill(json={'stopped':True})
            return
        if path.path == '/api/wifi/scan':
            if request.request.method=='POST':
                log.append(('scan',{}))
                state.update(armed=False,moving=False,mode='idle',wheels=[0]*4)
                await request.fulfill(json={'ok':True,'state':'scanning'})
            else:
                await request.fulfill(json={'state':'ready','networks':[{'ssid':'Lab "A" & room','rssi':-38,'open':False},{'ssid':'Open classroom','rssi':-65,'open':True}]})
            return
        if path.path == '/api/wifi/forget':
            log.append(('forget',{}))
            state.update(home_ssid='',saved_ssid='',home_settings_source='none',wifi_last_result='forgotten',wifi_join_state='idle',network_mode='hotspot',connected_ssid='Freenove-Rover',dashboard_url='http://192.168.4.1/',ip='192.168.4.1',armed=False,moving=False,mode='idle',wheels=[0]*4)
            await request.fulfill(json={'ok':True})
            return
        if path.path == '/api/wifi':
            values={k:v[0] for k,v in parse_qs(request.request.post_data,keep_blank_values=True).items()}
            assert path.query=='' and request.request.headers['x-car-token']=='test-token'
            assert request.request.headers['content-type']=='application/x-www-form-urlencoded'
            log.append(('wifi',values))
            if values['ssid']=='reject-this':
                await request.fulfill(status=400,json={'error':'Check Wi-Fi settings'})
            else:
                state.update(home_ssid=values['ssid'],saved_ssid=values['ssid'],connected_ssid=values['ssid'],home_settings_source='saved',wifi_last_result='joined',network_mode='home',wifi_join_state='joined',armed=False,moving=False,mode='idle',wheels=[0]*4)
                await request.fulfill(json={'ok':True})
            return
        values = {k:v[0] for k,v in parse_qs(path.query).items()}
        op = values.get('op')
        if op == 'drive' and delay_drive:
            delay_drive = False
            drive_pending.set()
            await asyncio.sleep(.3)
        log.append((op,values))
        if op == 'arm':
            state.update(armed=True,mode='manual')
        elif op == 'drive':
            state['moving'] = True
        elif op == 'halt':
            state.update(moving=False,wheels=[0]*4)
        elif op == 'matrix':
            face = values['face']
            state.update(matrix_face=face,matrix_active=face if face != 'auto' else 'eyes',
                matrix_brightness=int(values['brightness']),matrix_rotation=int(values['rotation']),matrix_rows=patterns[face])
        elif op == 'matrixhead':
            state.update(matrix_head_auto=values['enabled']=='1',scan_left_angle=int(values['left']),armed=False,moving=False,wheels=[0]*4)
        elif op == 'party':
            state.update(armed=False,moving=False,mode='idle',wheels=[0]*4,
                led_effect='party',matrix_source='party',matrix_active='happy',
                matrix_rows=patterns['happy'],buzzer_active=values['sound']=='1',buzzer_hz=523)
        elif op == 'melody':
            state.update(armed=False,moving=False,mode='idle',wheels=[0]*4,
                matrix_source='melody',matrix_active='happy',buzzer_active=True,buzzer_hz=523)
        elif op == 'network':
            state.update(network_mode=values['name'],armed=False,moving=False,mode='idle',wheels=[0]*4,
                ip='192.168.4.1' if values['name']=='hotspot' else '192.168.0.202')
        await request.fulfill(json={'ok':True})

    async with async_playwright() as playwright:
        browser = await playwright.chromium.launch()
        context = await browser.new_context(viewport={'width':390,'height':844},
            is_mobile=True,has_touch=True,device_scale_factor=1)
        await context.route('http://rover.test/**',route)
        page = await context.new_page()
        errors = []
        page.on('pageerror',lambda error:errors.append(str(error)))
        await page.goto('http://rover.test/')
        await page.wait_for_function("document.getElementById('connection').textContent==='Connected on your LAN'")
        cdp = await context.new_cdp_session(page)
        async def point(selector):
            box = await page.locator(selector).first.bounding_box()
            return dict(x=box['x']+box['width']/2,y=box['y']+box['height']/2,id=1)
        async def touch(kind,points):
            await cdp.send('Input.dispatchTouchEvent',{'type':kind,'touchPoints':points})
        async def arm():
            await page.locator('#arm').click()
            await page.wait_for_function("!document.querySelector('[data-drive]').disabled")
            await page.locator('#driveSurface').scroll_into_view_if_needed()
        def passed(name):
            results.append({'check':name,'passed':True})
            print('PASS:',name,flush=True)
        await arm()
        forward = await point('[aria-label="Forward"]')
        right = await point('[aria-label="Crab right"]')
        center = await point('#holdStop')
        log.clear()
        await touch('touchStart',[forward])
        await page.wait_for_timeout(1200)
        drives = [v for op,v in log if op=='drive']
        assert len(drives)>=5 and all(v['y']=='1' for v in drives)
        assert await page.evaluate('getSelection().toString()') == ''
        assert await page.locator('[aria-label="Forward"] small').evaluate(
            "e=>getComputedStyle(e).userSelect==='none'&&getComputedStyle(e).touchAction==='none'")
        assert '-webkit-touch-callout:none' in html  # Safari-specific; Chromium does not expose this property.
        passed('long touch keeps driving without selecting arrow text')
        await touch('touchMove',[right])
        await page.wait_for_timeout(180)
        assert [v for op,v in log if op=='drive'][-1]['x']=='1'
        await touch('touchMove',[center])
        await page.wait_for_timeout(180)
        assert [op for op,v in log if op in ('drive','halt')][-1]=='halt' and not state['moving']
        await touch('touchMove',[forward])
        await page.wait_for_timeout(150)
        await touch('touchMove',[dict(x=1,y=100,id=1)])
        await page.wait_for_timeout(150)
        assert [op for op,v in log if op in ('drive','halt')][-1]=='halt' and not state['moving']
        await touch('touchEnd',[])
        passed('sliding switches crab direction; neutral and outside stop')
        await touch('touchStart',[forward])
        await page.wait_for_timeout(120)
        await touch('touchCancel',[])
        await page.wait_for_timeout(150)
        assert not state['moving'] and not await page.locator('.held').count()
        passed('interrupted touch stops and clears the active button')
        await page.wait_for_function('!busy')
        await page.evaluate('motionQueue')
        log.clear()
        delay_drive = True
        await touch('touchStart',[forward])
        await asyncio.wait_for(drive_pending.wait(),1)
        await touch('touchEnd',[])
        await page.wait_for_timeout(550)
        motions=[op for op,v in log if op in ('drive','halt')]
        assert motions[0]=='drive' and motions[-1]=='halt' and not state['moving']
        passed('release stop remains ordered after an in-flight drive request')
        await touch('touchStart',[forward])
        await page.wait_for_timeout(120)
        await page.evaluate("switchTab('studio')")
        await touch('touchEnd',[])
        await page.wait_for_timeout(160)
        assert not state['moving']
        passed('changing panels releases manual movement')
        for face in ['happy','heart','angry','sad','wink','surprised','sleepy','cool','party','off','auto']:
            await page.locator('[data-face="'+face+'"]').click()
            await page.wait_for_timeout(90)
            assert state['matrix_face']==face
        await page.locator('#matrixBrightness').evaluate("e=>{e.value='9';e.dispatchEvent(new Event('change'))}")
        await page.wait_for_timeout(150)
        assert state['matrix_brightness']==9
        passed('expression gallery and brightness submit the selected setting')
        for angle in ['0','90','180','270']:
            await page.locator('#matrixRotation').select_option(angle)
            await page.wait_for_timeout(120)
            assert state['matrix_rotation']==int(angle)
            assert state['matrix_face']=='auto' and not state['moving']
        passed('all four display alignment choices submit without starting movement')
        await page.locator('#party').click()
        await page.wait_for_function("document.getElementById('matrixBadge').textContent==='party'")
        assert not state['armed'] and not state['moving']
        assert ('party',{'op':'party','sound':'1'}) in log
        assert 'Sound 523 Hz' in await page.locator('#matrixHelp').inner_text()
        await page.locator('#partySound').uncheck()
        await page.locator('#party').click()
        await page.wait_for_function("document.getElementById('matrixHelp').textContent.includes('Sound idle / muted')")
        assert not state['buzzer_active']
        await page.locator('#partySound').check()
        await page.locator('[data-face="party"]').click()
        await page.wait_for_timeout(180)
        assert state['matrix_face']=='party' and state['led_effect']=='party' and not state['armed']
        passed('stationary party and gallery shortcut share optional sound, live matrix feedback and stopped motors')
        await page.evaluate("switchTab('drive')")
        await arm()
        forward = await point('[aria-label="Forward"]')
        await touch('touchStart',[forward])
        await page.wait_for_timeout(120)
        await page.evaluate("window.dispatchEvent(new Event('blur'))")
        await touch('touchEnd',[])
        await page.wait_for_timeout(200)
        assert not state['armed'] and not state['moving']
        passed('window blur stops and disarms')
        await arm()
        await page.locator('[aria-label="Forward"]').focus()
        await page.keyboard.down('Space')
        await page.wait_for_timeout(120)
        assert state['moving']
        await page.keyboard.up('Space')
        await page.wait_for_timeout(160)
        assert not state['moving']
        passed('keyboard hold and release use the same stop behavior')
        for width in [320,390,1180]:
            await page.set_viewport_size({'width':width,'height':844})
            for tab in ['drive','studio']:
                await page.evaluate("switchTab('"+tab+"')")
                assert await page.evaluate('document.documentElement.scrollWidth<=innerWidth'),(width,tab)
        await page.locator('details').filter(has=page.locator('#networkInfo')).locator('summary').click()
        await page.locator('#hotspot').click()
        await page.wait_for_function("document.getElementById('connection').textContent==='Connected to car hotspot'")
        assert not state['armed'] and not state['moving']
        assert 'Freenove-Rover' in await page.locator('#networkInfo').inner_text()
        assert await page.locator('#hotspot').is_disabled()
        await page.locator('#homeWifi').click()
        await page.wait_for_function("document.getElementById('connection').textContent==='Connected on your LAN'")
        passed('network controls report direct/home connections and stop/disarm')
        assert 'Classroom' in await page.locator('#savedNetwork').inner_text()
        assert await page.locator('#networkUrl').get_attribute('href')=='http://192.168.0.202/'
        await page.locator('#wifiScan').click()
        await page.wait_for_function("document.getElementById('wifiNetworks').options.length===3")
        await page.locator('#wifiNetworks').select_option('0')
        assert await page.locator('#wifiSsid').input_value()=='Lab "A" & room'
        assert not await page.locator('#wifiOpen').is_checked()
        await page.locator('#wifiNetworks').select_option('1')
        assert await page.locator('#wifiOpen').is_checked()
        assert await page.locator('#wifiPassword').is_disabled()
        await page.locator('#wifiOpen').uncheck()
        passed('saved network, actual connection and URL display; discovered secure/open networks fill the form safely')
        await page.set_viewport_size({'width':390,'height':844})
        await page.locator('#wifiNetworks').select_option('0')
        screenshot_style=await page.add_style_tag(content='.tabs{position:static!important}.dock{display:none!important}')
        await page.locator('#networkSettings').screenshot(path=str(root/'docs/images/dashboard-wifi-settings.png'))
        await screenshot_style.evaluate('(element)=>element.remove()')
        await page.locator('#wifiSsid').fill('Classroom + & lab')
        await page.locator('#wifiPassword').fill('a+b&c%123')
        await page.locator('#wifiShow').check()
        assert await page.locator('#wifiPassword').get_attribute('type')=='text'
        await page.locator('#wifiJoin').click()
        await page.wait_for_function("document.getElementById('wifiPassword').value===''")
        assert log[-1]==('wifi',{'ssid':'Classroom + & lab','password':'a+b&c%123','open':'0'})
        assert not state['armed'] and not state['moving']
        await page.locator('#wifiSsid').fill('Open classroom')
        await page.locator('#wifiOpen').check()
        await page.locator('#wifiJoin').click()
        await page.wait_for_timeout(120)
        assert log[-1]==('wifi',{'ssid':'Open classroom','password':'','open':'1'})
        await page.locator('#wifiOpen').uncheck()
        await page.locator('#wifiSsid').fill('reject-this')
        await page.locator('#wifiPassword').fill('eight123')
        await page.locator('#wifiJoin').click()
        await page.wait_for_function("document.getElementById('wifiResult').textContent==='Check Wi-Fi settings'")
        assert await page.locator('#wifiPassword').input_value()=='eight123'
        assert not await page.locator('#wifiJoin').is_disabled()
        for width in [320,390,1180]:
            await page.set_viewport_size({'width':width,'height':844})
            assert await page.evaluate('document.documentElement.scrollWidth<=innerWidth')
        passed('Wi-Fi form posts special characters privately, clears accepted passwords, supports open networks and reports rejected settings')
        page.on('dialog',lambda dialog:asyncio.create_task(dialog.accept()))
        await page.locator('#wifiForget').click()
        await page.wait_for_function("document.getElementById('savedNetwork').textContent==='No home network saved'")
        await page.reload()
        await page.wait_for_function("document.getElementById('savedNetwork').textContent==='No home network saved'")
        assert await page.locator('#homeWifi').is_disabled()
        assert await page.locator('#wifiForget').is_disabled()
        passed('forget action stops the car and deleted state remains clear after dashboard reload')
        await page.evaluate("switchTab('studio')")
        await page.locator('#matrixHeadAuto').uncheck()
        await page.wait_for_function("document.getElementById('matrixHeadAuto').checked===false")
        assert not state['matrix_head_auto']
        await page.locator('#matrixHeadLeft').select_option('120')
        await page.locator('#matrixHeadAuto').check()
        await page.wait_for_timeout(300)
        assert state['matrix_head_auto'] and state['scan_left_angle']==120 and not state['moving']
        passed('interactive matrix head toggle and mounting direction submit without starting wheels')
        assert not errors,errors
        passed('drive and expression panels fit 320px, 390px and desktop widths; no script errors')
        await browser.close()
    (root/'build/mobile-dashboard-test.json').write_text(json.dumps(results,indent=2))

asyncio.run(run())

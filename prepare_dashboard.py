"""Embed the local, dependency-free dashboard in the Pico sketch."""
from pathlib import Path
source=Path(__file__).parent/'firmware'/'CarReady'/'dashboard.html'
html=source.read_text(encoding='utf-8')
assert ')HTML"' not in html and html.count('__TOKEN__')==1
assert len(html.encode())<49152, 'Keep the streamed dashboard within the flash and LAN budget'
source.with_name('Dashboard.h').write_text('#pragma once\nconst char dashboard[] PROGMEM=R"HTML('+html+')HTML";\n',encoding='utf-8')

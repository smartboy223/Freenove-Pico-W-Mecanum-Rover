// One captured pointer owns motion. Sliding to neutral/outside stops immediately.
function createTouchDrive(surface,{enabled,onDrive,onRelease}) {
  let pointer=null,last='',marked=null;
  function mark(button){if(marked)marked.classList.remove('held');marked=button;if(marked)marked.classList.add('held')}
  function neutral(){if(last){last='';mark(null);onRelease()}}
  function cancel(){let id=pointer;pointer=null;neutral();if(id!==null&&surface.hasPointerCapture(id))surface.releasePointerCapture(id)}
  function target(e){let node=surface.ownerDocument.elementFromPoint(e.clientX,e.clientY);let button=node&&node.closest('[data-drive]');return button&&surface.contains(button)&&!button.disabled?button:null}
  function move(e){if(pointer!==e.pointerId)return;e.preventDefault();if(!enabled()){cancel();return}let button=target(e);if(!button){neutral();return}let value=button.dataset.drive;if(value!==last){last=value;mark(button);onDrive(value.split(',').map(Number),button.getAttribute('aria-label'))}}
  surface.addEventListener('pointerdown',e=>{if(pointer!==null||!enabled()||e.isPrimary===false||(e.pointerType==='mouse'&&e.button!==0))return;let button=target(e);if(!button)return;e.preventDefault();pointer=e.pointerId;try{surface.setPointerCapture(pointer)}catch(error){pointer=null;return}move(e)});
  surface.addEventListener('pointermove',move);
  surface.addEventListener('pointerup',e=>{if(e.pointerId===pointer){e.preventDefault();cancel()}});
  surface.addEventListener('pointercancel',e=>{if(e.pointerId===pointer)cancel()});
  surface.addEventListener('lostpointercapture',e=>{if(e.pointerId===pointer)cancel()});
  surface.addEventListener('contextmenu',e=>{e.preventDefault();cancel()});
  surface.addEventListener('selectstart',e=>e.preventDefault());
  surface.addEventListener('dragstart',e=>e.preventDefault());
  // Prevent the iOS long-touch callout on nested arrow/label elements.
  surface.addEventListener('touchstart',e=>{if(pointer!==null)e.preventDefault()},{passive:false});
  surface.addEventListener('keydown',e=>{if(![' ','Enter'].includes(e.key)||!enabled())return;let button=e.target.closest('[data-drive]');if(!button||button.disabled||e.repeat||pointer!==null)return;e.preventDefault();last=button.dataset.drive;mark(button);onDrive(last.split(',').map(Number),button.getAttribute('aria-label'))});
  surface.addEventListener('keyup',e=>{if([' ','Enter'].includes(e.key)){e.preventDefault();neutral()}});
  surface.addEventListener('focusout',()=>{if(pointer===null)neutral()});
  return {cancel};
}

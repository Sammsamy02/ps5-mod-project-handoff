/* Controller-only text entry. Drafts touch the field only after Done. */
(() => {
 'use strict';
 let dialog, target, draft = '', shifted = false, returnFocus;
 const esc = value => String(value).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
 const focus = el => { if(el?.isConnected){el.focus({preventScroll:true});el.scrollIntoView({block:'nearest',inline:'nearest'});} };
 const limit = () => target?.maxLength >= 0 ? target.maxLength : 2048;
 const close = apply => {
  if(!dialog?.open)return;
  const input=target;
  if(apply && input?.isConnected){input.value=draft;input.dispatchEvent(new Event('input',{bubbles:true}));input.dispatchEvent(new Event('change',{bubbles:true}));}
  dialog.close();target=null;focus(returnFocus);returnFocus=null;
 };
 const update = () => {
  dialog.querySelector('output').textContent=draft || 'Your text will appear here';
  dialog.querySelector('[data-keyboard-count]').textContent=`${draft.length} of ${limit()} characters`;
 };
 const render = () => {
  const activeKey=dialog.querySelector(':focus')?.dataset.key;
  const rows=['1234567890','qwertyuiop','asdfghjkl','zxcvbnm','./:-_?=&'];
  dialog.innerHTML=`<div class="dialog-head"><h2 id="keyboard-title">${esc(target.labels?.[0]?.textContent?.trim() || target.getAttribute('aria-label') || 'Enter text')}</h2><button class="secondary-button" data-key="Cancel">Cancel</button></div><output class="keyboard-draft" tabindex="0" data-controller-scroll aria-label="Text being entered" aria-live="polite"></output><p class="content-note" data-keyboard-count></p><div class="keyboard-keys">${rows.map(row=>`<div class="keyboard-row">${[...row].map(key=>{const value=shifted?key.toUpperCase():key;return `<button class="secondary-button keyboard-key" data-key="${esc(value)}">${esc(value)}</button>`;}).join('')}</div>`).join('')}</div><div class="installer-actions keyboard-actions"><button class="secondary-button" data-key="Shift" aria-pressed="${shifted}">Shift</button><button class="secondary-button" data-key="Space">Space</button><button class="secondary-button" data-key="Backspace">Backspace</button><button class="secondary-button" data-key="Clear">Clear</button><button class="action-button" data-key="Done">Done</button></div><p class="content-note">✕ Select · ○ Cancel · Your original text stays until you choose Done.</p>`;
  update();
  dialog.querySelectorAll('[data-key]').forEach(button=>button.addEventListener('click',()=>{
   const key=button.dataset.key;
   if(key==='Done')return close(true);
   if(key==='Cancel')return close(false);
   if(key==='Shift'){shifted=!shifted;render();return;}
   if(key==='Clear')draft='';
   else if(key==='Backspace')draft=[...draft].slice(0,-1).join('');
   else {const value=key==='Space'?' ':key;if(draft.length+value.length<=limit())draft+=value;}
   update();
  }));
  if(dialog.open)focus([...dialog.querySelectorAll('[data-key]')].find(el=>el.dataset.key===activeKey) || dialog.querySelector('[data-key]'));
 };
 const open = input => {
  if(!input?.matches('input[type="text"],input[type="search"],input[type="url"],input:not([type]),textarea') || input.disabled || input.readOnly || dialog?.open)return false;
  if(!dialog){dialog=document.createElement('dialog');dialog.id='controller-keyboard';dialog.className='controller-keyboard';dialog.setAttribute('aria-labelledby','keyboard-title');document.body.append(dialog);dialog.addEventListener('cancel',event=>{event.preventDefault();close(false);});}
  target=input;returnFocus=input;draft=input.value.slice(0,input.maxLength>=0?input.maxLength:2048);shifted=false;render();dialog.showModal();focus(dialog.querySelector('[data-key="1"]'));return true;
 };
 window.ModBridgeKeyboard=Object.freeze({open});
})();

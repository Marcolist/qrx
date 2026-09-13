const $=id=>document.getElementById(id), log=x=>$('log').textContent=typeof x==='string'?x:JSON.stringify(x,null,2);
$('identityBtn').onclick=async()=>{try{const x=await QRX.wallet.getIdentity();$('identity').textContent=JSON.stringify(x,null,2);}catch(e){log(String(e));}};
$('balanceBtn').onclick=async()=>{try{log(await QRX.wallet.getBalance());}catch(e){log(String(e));}};
$('heightBtn').onclick=async()=>{try{$('chain').textContent=JSON.stringify(await QRX.chain.getHeight(),null,2);}catch(e){log(String(e));}};
$('networkBtn').onclick=async()=>{try{log(await QRX.network.getStatus());}catch(e){log(String(e));}};
$('saveBtn').onclick=async()=>{try{log(await QRX.storage.set('note',$('note').value));}catch(e){log(String(e));}};
$('loadBtn').onclick=async()=>{try{$('note').value=(await QRX.storage.get('note'))||'';}catch(e){log(String(e));}};
$('payBtn').onclick=async()=>{try{log(await QRX.wallet.requestPayment({recipient:$('recipient').value,amount:$('amount').value,memo:'Hello QRX demo'}));}catch(e){log(String(e));}};

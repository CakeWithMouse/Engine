"""Read-only triage: candidate return addresses, NOT an unwound stack."""
import struct,sys,ctypes,os
from pathlib import Path
b=Path(sys.argv[1]).read_bytes()
u32=lambda o:struct.unpack_from('<I',b,o)[0]
u64=lambda o:struct.unpack_from('<Q',b,o)[0]
streams={u32(o):(u32(o+4),u32(o+8)) for o in range(u32(12),u32(12)+u32(8)*12,12)}
_,m=streams[4];mods=[]
for o in range(m+4,m+4+u32(m)*108,108):
 p=u32(o+20); name=b[p+4:p+4+u32(p)].decode('utf-16le');mods.append((u64(o),u32(o+8),name))
_,e=streams[6];ctx=u32(e+164);rsp=u64(ctx+152);rip=u64(ctx+248)
print('thread',u32(e),'RIP',hex(rip),'RSP',hex(rsp))
segments=[]
if 5 in streams:
 _,m=streams[5]
 for o in range(m+4,m+4+u32(m)*16,16):segments.append((u64(o),u32(o+8),u32(o+12)))
if 9 in streams:
 _,m=streams[9];pos=u64(m+8)
 for o in range(m+16,m+16+u64(m)*16,16):
  sz=u64(o+8);segments.append((u64(o),sz,pos));pos+=sz
d=ctypes.WinDLL('dbghelp');kernel=ctypes.WinDLL('kernel32');handle=kernel.GetCurrentProcess()
d.SymInitializeW.argtypes=[ctypes.c_void_p,ctypes.c_wchar_p,ctypes.c_int];d.SymInitializeW(handle,'E:/Engine/Game/build/x64/Release/Runtime',0)
d.SymLoadModuleExW.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_wchar_p,ctypes.c_wchar_p,ctypes.c_ulonglong,ctypes.c_ulong,ctypes.c_void_p,ctypes.c_ulong]
d.SymLoadModuleExW.restype=ctypes.c_ulonglong
for base,size,name in mods:d.SymLoadModuleExW(handle,None,name,None,base,size,None,0)
d.SymFromAddr.argtypes=[ctypes.c_void_p,ctypes.c_ulonglong,ctypes.POINTER(ctypes.c_ulonglong),ctypes.c_void_p]
def desc(addr):
 for base,size,name in mods:
  if base<=addr<base+size:
   buf=ctypes.create_string_buffer(2200);struct.pack_into('<I',buf,0,88);struct.pack_into('<I',buf,80,2000);delta=ctypes.c_ulonglong()
   symbol=''
   if d.SymFromAddr(handle,addr,ctypes.byref(delta),buf):symbol=buf.raw[84:].split(b'\0')[0].decode(errors='replace')+'+'+hex(delta.value)
   return f'{Path(name).name}+{addr-base:#x} {symbol}'
 return None
print('FAULT:',desc(rip))
for base,size,filepos in segments:
 if base<=rsp<base+size:
  for offset in range(0,min(8192,base+size-rsp),8):
   addr=u64(filepos+rsp-base+offset);s=desc(addr)
   if s:print(f'RSP+{offset:#x}: {s}')
  break

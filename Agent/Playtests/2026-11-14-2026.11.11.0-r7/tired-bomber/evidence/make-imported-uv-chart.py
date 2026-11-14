import struct,zlib,pathlib
p=pathlib.Path(r'E:\Engine\Engine\build\playtests\20261114-r7\tired-bomber\final-b-graphics\UvChart.png')
w=h=128
rows=[]
colors=((240,24,20,255),(20,220,40,255),(24,40,240,255),(240,220,20,255))
for y in range(h):
 row=bytearray([0])
 for x in range(w):
  c=colors[(1 if x>=64 else 0)+(2 if y>=64 else 0)]
  row.extend(c)
 rows.append(bytes(row))
def chunk(t,d): return struct.pack('>I',len(d))+t+d+struct.pack('>I',zlib.crc32(t+d)&0xffffffff)
p.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(b''.join(rows),9))+chunk(b'IEND',b''))

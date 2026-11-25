import sys
from PIL import Image
ref=open(sys.argv[1],'rb').read(); out=open(sys.argv[2],'rb').read()
import os
stp=sys.argv[4] if len(sys.argv)>4 else os.path.join(os.path.dirname(sys.argv[1]),'state.bin')
st=open(stp,'rb').read(); has=st[4+544+1024+65536:4+544+1024+65536+224]
W,H=256,224
bad=0; rows=set(); maxd=0; over=0; TOL=int(os.environ.get('TOL','2'))
for i in range(W*H):
    if has[i//W] and ref[i*4:i*4+3]!=out[i*4:i*4+3]:
        bad+=1; rows.add(i//W)
        d=max(abs(ref[i*4+k]-out[i*4+k]) for k in range(3)); maxd=max(maxd,d); over+= d>TOL
print("maxdiff:",maxd,"over tol:",over,"| handled rows:",sum(1 for x in has if x),"mismatching pixels:",bad,"of",W*H,"rows:",len(rows), (min(rows),max(rows)) if rows else "")
def img(b):
    im=Image.frombytes("RGBA",(W,H),b); im=im.convert("RGB"); return im
a=img(ref); b=img(out)
c=Image.new("RGB",(W*2+4,H)); c.paste(a,(0,0)); c.paste(b,(W+4,0))
c=c.resize((c.width*2,c.height*2),Image.NEAREST); c.save(sys.argv[3])

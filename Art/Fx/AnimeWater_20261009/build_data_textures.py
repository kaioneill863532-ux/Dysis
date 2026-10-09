import pathlib,json,collections,numpy as np
from PIL import Image,ImageDraw
root=pathlib.Path(__file__).parent;(root/'textures').mkdir(exist_ok=True)
geo=json.loads((root/'geometry.json').read_text())['SM_Mech_Water_Pool'];v=np.array(geo['vertices']);tri=np.array(geo['triangles'])
N=1024;lo=-1100.;span=2200.;pix=span/N
im=Image.new('L',(N,N),0);draw=ImageDraw.Draw(im)
for t in tri:draw.polygon([tuple((v[i,:2]-lo)/span*(N-1)) for i in t],fill=255)
# Image row = world Y; material samples the same world-plane coordinates.
coverage=np.array(im)>0;X,Y=np.meshgrid(lo+(np.arange(N)+.5)*pix,lo+(np.arange(N)+.5)*pix)
counts=collections.Counter(tuple(sorted((int(t[i]),int(t[(i+1)%3])))) for t in tri for i in range(3));edges=[e for e,c in counts.items() if c==1]
dist=np.full((N,N),1e9)
for ia,ib in edges:
    a=v[ia,:2];b=v[ib,:2];d=b-a;t=np.clip(((X-a[0])*d[0]+(Y-a[1])*d[1])/np.dot(d,d),0,1)
    dist=np.minimum(dist,np.hypot(X-a[0]-t*d[0],Y-a[1]-t*d[1]))
warm=np.exp(-((X+180)/310)**2-((Y+375)/540)**2)*coverage
shallow=np.clip(1-dist/180,0,1)*(.85+.15*np.sin(X/520+Y/720))
data=np.stack([np.clip(dist/200,0,1),shallow,np.clip(warm,0,1),coverage],axis=-1)
Image.fromarray(np.uint8(np.clip(data,0,1)*255),'RGBA').save(root/'textures/T_DYSIS_PoolShore.png')
# Flow is mostly translational; only a narrow shoreline zone bends tangentially.
dy,dx=np.gradient(dist,pix);mag=np.maximum(np.hypot(dx,dy),.001);tx=-dy/mag;ty=dx/mag
sgn=np.where(tx*.85+ty*.52<0,-1,1);w=np.clip(1-dist/90,0,1)*.65
fx=.85*(1-w)+tx*sgn*w;fy=.52*(1-w)+ty*sgn*w;mag=np.maximum(np.hypot(fx,fy),.001)
flow=np.stack([fx/mag*.5+.5,fy/mag*.5+.5,np.zeros_like(X),np.ones_like(X)],axis=-1)
Image.fromarray(np.uint8(np.clip(flow,0,1)*255),'RGBA').save(root/'textures/T_DYSIS_PoolFlow.png')
P=512;x,y=np.meshgrid((np.arange(P)+.5)/P,(np.arange(P)+.5)/P);pi=np.pi
field=(np.sin(2*pi*(x+.17))+np.sin(2*pi*(y+.42))*.65+np.cos(2*pi*(x+y+.28))*.35)/2
big=np.clip(.5+field*.55,0,1);big=(np.floor(big*3)+.5)/3;big=np.clip(big,0,1)
lines=np.zeros_like(x)
for k in range(5):
    yc=k/5+.04*np.sin(2*pi*x+(k*1.93))+.012*np.cos(4*pi*x+k)
    dyw=(y-yc+.5)%1-.5
    line=np.clip((.0105-np.abs(dyw))/.004,0,1)
    gaps=np.clip((np.sin(2*pi*(3*x+.4*k))-.35)*7,0,1)
    lines=np.maximum(lines,line*gaps)
warp=.5+.5*(np.sin(2*pi*x+.7)*np.cos(2*pi*y+.8))
pat=np.stack([big,lines,warp,np.ones_like(big)],axis=-1)
Image.fromarray(np.uint8(pat*255),'RGBA').save(root/'textures/T_DYSIS_WaterShapes.png')
(root/'mask_metadata.json').write_text(json.dumps({'source':'actual UE LOD0 world-space triangles','water_vertices':len(v),'water_triangles':len(tri),'boundary_edges':len(edges),'extent_min_cm':[lo,lo],'extent_span_cm':span,'water_z_cm':-45,'mask_R':'distance to real annulus boundary / 200cm, clamped','mask_G':'art-directed shallow region, not measured water depth','mask_B':'localized peach reflection envelope near pavilion','mask_A':'actual water coverage for inspection; mesh owns visible range','flow_RG':'linear data, decoded [-1,1], translation + 90cm shoreline tangent blend','pattern_repeat_cm':400},indent=2),encoding='utf-8')
print('BUILT_GEOMETRY_MASKS',len(edges),coverage.sum())

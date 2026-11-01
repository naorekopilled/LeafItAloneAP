# Dumps name, script reference and raw bytes of every ScriptableObject in coreassets bundles.
import sys,os,json,glob,re
import UnityPy
root,out=sys.argv[1:3]
res={}
for p in sorted(glob.glob(root+'/coreassets_assets_*.bundle')):
    if os.path.getsize(p)>2_000_000: continue
    e=UnityPy.load(p); short=re.sub(r'coreassets_assets_|_[0-9a-f]{32}\.bundle','',os.path.basename(p))
    rows=[]
    for o in e.objects:
        if o.type.name!='MonoBehaviour': continue
        r=o.get_raw_data() if hasattr(o,'get_raw_data') else None
        raw=bytes(r) if r is not None else b''
        try:
            t=o.read_typetree()
        except Exception as x:
            t=None
        rows.append({'pid':o.path_id,'tt':t if isinstance(t,dict) else None,'raw':raw.hex()})
    res[short]=rows
json.dump(res,open(out,'w'))
print({k:len(v) for k,v in res.items()})

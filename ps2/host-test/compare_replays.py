#!/usr/bin/env python3
"""Compare rendered pixels against a pristine host build. No PS2 timing claims."""
import argparse, concurrent.futures, hashlib, json, os, pathlib, re, subprocess
p=argparse.ArgumentParser();p.add_argument('baseline',type=pathlib.Path);p.add_argument('candidate',type=pathlib.Path);p.add_argument('assets',type=pathlib.Path);p.add_argument('output',type=pathlib.Path);p.add_argument('--frames',type=int,default=2500);a=p.parse_args()
root=pathlib.Path(__file__).resolve().parents[2]
out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
config=(root/'ps2/zelda3.ini').read_text();config=re.sub(r'OutputMethod\s*=.*', 'OutputMethod = SDL-Software',config);config=re.sub(r'DisableFrameDelay\s*=.*','DisableFrameDelay = 1',config);config=re.sub(r'EnableAudio\s*=.*','EnableAudio = 0',config)
base=a.baseline.resolve();candidate=a.candidate.resolve();assets=a.assets.resolve()
def run(scene):
    case='intro' if scene<0 else f'chapter{scene+1:02}'
    paths=[];logs=[]
    for label,exe in [('baseline',base),('candidate',candidate)]:
        d=out/f'{case}-{label}';d.mkdir(exist_ok=True);(d/'saves').mkdir(exist_ok=True)
        for target,source in [(d/'zelda3_assets.dat',assets),(d/'saves/ref',root/'saves/ref')]:
            if not target.exists():target.symlink_to(source)
        (d/'zelda3.ini').write_text(config)
        env=dict(os.environ, SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',Z3_FRAMES=str(a.frames),Z3_HASH=str(d/'frames.hash'),UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=0')
        env.pop('Z3_REPLAY',None)
        if scene>=0:env['Z3_REPLAY']=str(scene)
        result=subprocess.run([str(exe)],cwd=d,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=240)
        (d/'run.log').write_bytes(result.stdout)
        assert result.returncode==0,(case,label,result.returncode,result.stdout[-3000:])
        assert b'runtime error' not in result.stdout,(case,label,result.stdout[-3000:])
        hashes=(d/'frames.hash').read_bytes();assert len(hashes.splitlines())==a.frames,(case,label,len(hashes.splitlines()))
        paths.append(hashes);logs.append(result.stdout)
    assert paths[0]==paths[1],f'Pixel mismatch: {case}'
    indexed=re.findall(rb'GS indexed scanlines: (\d+)',logs[1])
    result={'scene':case,'frames':a.frames,'identical':True,'indexed_scanlines':int(indexed[-1]) if indexed else 0,'hash_sha256':hashlib.sha256(paths[1]).hexdigest()}
    print(json.dumps(result),flush=True);return result
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:results=list(pool.map(run,range(-1,13)))
(out/'results.json').write_text(json.dumps(results,indent=2)+'\n')
print(f'PASS: {len(results)*a.frames} frames, no pixel mismatch, no alignment errors',flush=True)

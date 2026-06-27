import subprocess, os
os.chdir(r'C:\\dev\\phd_code\\c')
CORE='prover.c verifier.c circuit.c ntt.c merkle.c transcript.c sha256.c'.split()
FIELDS=['babybear','koalabear','goldilocks']
MLNS=[4,6,8,10,12]
rows=[]
for field in FIELDS:
    for mln in MLNS:
        exe='sw.exe'
        cc=['gcc','-O2','-std=c11','-I.','-DMAX_LOG_N=%d'%mln,'-DFIELD_%s'%field.upper(),'-o',exe,'prove_main.c']+CORE
        r=subprocess.run(cc,capture_output=True,text=True)
        if r.returncode!=0:
            rows.append((field,mln,'BUILDFAIL',None)); continue
        p=subprocess.run([exe,'sw.bin'],capture_output=True,text=True,timeout=120)
        out=p.stdout
        total=None; ok=(p.returncode==0)
        for line in out.splitlines():
            if 'TOTAL' in line:
                total=int(''.join(ch for ch in line.split(':')[1] if ch.isdigit()))
        rows.append((field,mln,'ok' if ok else 'RUNFAIL',total))
        print(field,mln,total,'KB',rows[-1][2])
import json
open('sweep_results.json','w').write(json.dumps(rows))
print('DONE')
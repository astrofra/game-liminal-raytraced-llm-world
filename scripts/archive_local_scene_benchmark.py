"""Archive measured scene images and a labelled comparison sheet (requires Pillow)."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess

from PIL import Image, ImageDraw, ImageFont
from scene_local_report import write_report

ROOT=Path(__file__).resolve().parents[1]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('campaign',type=Path)
    parser.add_argument('destination',type=Path,help='New archive directory')
    parser.add_argument('--render-rejected',action='store_true',help='Diagnostic images only; never changes acceptance or measured time')
    args=parser.parse_args()
    records=json.loads((args.campaign/'results.json').read_text(encoding='utf-8'))
    environment=json.loads((args.campaign/'environment.json').read_text(encoding='utf-8'))
    destination=args.destination.resolve();destination.mkdir(parents=True,exist_ok=False)
    models=list(dict.fromkeys(r['model'] for r in records))
    rows=list(dict.fromkeys((r['case_id'],r['trial']) for r in records))
    tiles={}
    copied=[]
    font=ImageFont.truetype('C:/Windows/Fonts/arial.ttf',15)
    for record in records:
        model_index=models.index(record['model'])
        source=args.campaign/f'model-{model_index}'/f"{record['case_id']}-{record['trial']}"
        target=destination/f'model-{model_index}'/source.name;target.mkdir(parents=True)
        attempt_paths=[]
        for scene in sorted(source.glob('attempt-*.scene')):
            shutil.copy2(scene,target/scene.name)
            image=scene.with_suffix('.png')
            if image.exists():
                shutil.copy2(image,target/image.name)
                attempt_paths.append(target/image.name)
        rejected_render=None
        if not record['accepted'] and args.render_rejected:
            scenes=sorted(target.glob('attempt-*.scene'))
            if scenes:
                rejected_render=target/'rejected-diagnostic.png'
                process=subprocess.run([str(ROOT/'build/Release/liminal_cornell_renderer.exe'),
                    '--scene',str(scenes[-1]),'--output',str(rejected_render),
                    '--width','800','--height','400','--samples','8','--bounces','3','--direct-samples','1','--seed','77'],
                    cwd=ROOT,capture_output=True,timeout=30)
                if process.returncode or not rejected_render.exists():rejected_render=None
        selected=attempt_paths[-1] if attempt_paths else rejected_render
        measured={k:record.get(k) for k in ('model','case_id','trial','accepted','first_pass','total_ms','render_ms','calls','error','scene_sha256')}
        measured['image_path']=str(selected) if selected else None
        measured['diagnostic_image_only']=bool(rejected_render)
        copied.append(measured)
        tile=Image.new('RGB',(500,300),(18,22,25))
        if selected:
            with Image.open(selected) as image:tile.paste(image.resize((500,250)),(0,50))
        draw=ImageDraw.Draw(tile)
        status='ACCEPTE' if record['accepted'] else 'REJETE / diagnostic hors mesure'
        draw.text((8,4),f"{record['model']} | {record['total_ms']/1000:.2f}s | {status}",font=font,
                  fill=(120,240,160) if record['accepted'] else (255,160,130))
        draw.text((8,26),f"{record['case_id']} / essai {record['trial']+1}",font=font,fill='white')
        tiles[(record['case_id'],record['trial'],record['model'])]=tile
    sheet=Image.new('RGB',(500*len(models),300*len(rows)),(10,12,15))
    for y,(case,trial) in enumerate(rows):
        for x,model in enumerate(models):
            tile=tiles.get((case,trial,model))
            if tile:sheet.paste(tile,(500*x,300*y))
    sheet.save(destination/'comparison.png')
    write_report(copied,destination/'REPORT.md')
    # Portable evidence: no user profiles, requests, transcripts or absolute paths.
    for record in copied:
        if record['image_path']:record['image_path']=Path(record['image_path']).relative_to(destination).as_posix()
    (destination/'measurements.json').write_text(json.dumps(copied,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
    manifest={'ollama_version':environment['version'],'renderer_sha256':environment['renderer_sha256'],
              'cases_sha256':environment['cases_sha256'],'models':{}}
    for name,value in environment['models'].items():
        info=value['show']
        manifest['models'][name]={'details':info.get('details'),'parameters':info.get('parameters'),
            'thinking_requested':value.get('thinking_requested'),'cold_wall_ms':value.get('cold_wall_ms'),
            'cold_metrics':value.get('cold'),'residency':value.get('residency'),
            'digest':next((m['digest'] for m in environment['tags']['models'] if m['name']==name),None)}
    (destination/'manifest.json').write_text(json.dumps(manifest,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
    print(destination/'comparison.png')


if __name__=='__main__':main()

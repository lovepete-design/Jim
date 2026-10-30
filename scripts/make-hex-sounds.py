#!/usr/bin/env python3
"""Encode original OP1-first voice recipes into strict DX7 VCED SysEx files."""
from pathlib import Path
import argparse
import json
import re
import shutil
import zipfile

ROOT = Path(__file__).resolve().parent.parent

def number(value, upper, label):
    if not isinstance(value, int) or isinstance(value, bool) or not 0 <= value <= upper:
        raise ValueError(f"{label}: expected integer 0..{upper}, got {value!r}")
    return value

def four(value, label):
    if not isinstance(value, list) or len(value) != 4:
        raise ValueError(f"{label} needs four stages")
    return [number(x, 99, label) for x in value]

def encode(voice):
    patch = bytearray(155)
    assert len(voice['operators']) == 6
    for human_op, operator in enumerate(voice['operators'], 1):
        offset = (6 - human_op) * 21
        patch[offset:offset+4] = bytes(four(operator['rates'], 'rates'))
        patch[offset+4:offset+8] = bytes(four(operator['levels'], 'levels'))
        if patch[offset+7] != 0:
            raise ValueError('Every voice must have a silent final release level')
        fields = [('breakpoint',50,99),('left_depth',0,99),('right_depth',0,99),
                  ('left_curve',0,3),('right_curve',0,3),('rate_scaling',0,7),
                  ('amp_mod',0,3),('velocity',2,7),('level',0,99),
                  ('mode',0,1),('coarse',1,31),('fine',0,99),('detune',7,14)]
        for index, (key, default, maximum) in enumerate(fields,8):
            patch[offset+index] = number(operator.get(key,default),maximum,key)
    pitch = voice.get('pitch_eg', {'rates':[99]*4, 'levels':[50]*4})
    patch[126:130] = bytes(four(pitch['rates'],'pitch rates'))
    patch[130:134] = bytes(four(pitch['levels'],'pitch levels'))
    patch[134] = number(voice['algorithm']-1,31,'algorithm')
    patch[135] = number(voice.get('feedback',0),7,'feedback')
    patch[136] = number(voice.get('osc_sync',1),1,'oscillator sync')
    lfo = voice.get('lfo',{})
    for index,(key,default,maximum) in enumerate([
            ('speed',30,99),('delay',0,99),('pmd',0,99),('amd',0,99),
            ('sync',1,1),('wave',4,5),('pms',0,7)],137):
        patch[index] = number(lfo.get(key,default),maximum,'LFO '+key)
    patch[144] = number(voice.get('transpose',24),48,'transpose')
    name = voice['name']
    if not 1 <= len(name) <= 10 or any(not 32 <= ord(x) <= 126 for x in name):
        raise ValueError(f'Voice name must be 1..10 ASCII characters: {name}')
    patch[145:155] = name.ljust(10).encode('ascii')
    return bytes([0xf0,0x43,0,0,1,0x1b])+patch+bytes([(-sum(patch))&127,0xf7])

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--output',type=Path,default=ROOT/'dist'/'Hex-MkI-Sounds-Vol1')
    parser.add_argument('--zip',type=Path)
    args=parser.parse_args()
    voices=[]
    for name in ('keys.json','bass-leads.json','bells-pads.json'):
        voices.extend(json.loads((ROOT/'presets'/'vol1'/name).read_text()))
    if len(voices)!=16 or len({v['name'] for v in voices})!=16:
        raise ValueError('Expected 16 uniquely named original voices')
    args.output.mkdir(parents=True,exist_ok=True)
    patches=args.output/'Sounds'
    patches.mkdir(exist_ok=True)
    manifest=[]
    for index,voice in enumerate(voices,1):
        filename=f"{index:02d} - {re.sub(r'[^A-Za-z0-9 -]','',voice['label'])}.syx"
        data=encode(voice)
        assert len(data)==163 and sum(data[6:162])%128==0
        (patches/filename).write_bytes(data)
        manifest.append({**voice,'file':filename})
    (args.output/'voices.json').write_text(json.dumps(manifest,indent=2)+'\n')
    (args.output/'voices.tsv').write_text(''.join(f"{v['file']}\t{v['demo']}\n" for v in manifest))
    lines=['# Hex MkI — Original Sounds Vol. 1','','16 new voices for your existing Hex MkI instrument. No plugin reinstall is needed.','',
           '## Load a sound in Ableton','','1. Open Hex MkI on a MIDI track.','2. Click **Import DX7 SysEx**.','3. Open one `.syx` file from the **Sounds** folder.','4. Play your MIDI keyboard. Start with Output around **-12 dB**.','',
           'Save your Ableton Set to retain the imported voice. You can also save the device as an Ableton preset for quick recall. Imported voices do not add entries to Hex MkI\'s original three-item preset menu.','',
           '## Sounds','','| File | Character |','| --- | --- |']
    lines += [f"| {v['file']} | {v['description']} |" for v in manifest]
    lines += ['','Bass voices suit low notes; keys and bells suit the middle and upper register. Pads respond best to held chords. Velocity changes brightness as well as level in many voices.','',
              'If included, **Listen** contains dry audio examples rendered by the same Hex MkI DX7 engine, without added effects.','',
              '## Format and usage','','Each file is a 163-byte Yamaha-format DX7 single-voice VCED SysEx dump with a valid checksum. These are original parameter sets, not extracted factory presets. The voice data is dedicated to the public domain under CC0-1.0; use it freely in your music. No license restriction is placed on music made using these sounds.','',
              'The JSON recipes are included for editing and reproduction. Yamaha/DX7 identify the compatible format; no manufacturer endorsement is implied.']
    (args.output/'README.md').write_text('\n'.join(lines)+'\n')
    (args.output/'Voice-Recipes.json').write_text(json.dumps(voices,indent=2)+'\n')
    shutil.copyfile(ROOT/'presets'/'vol1'/'LICENSE.txt',args.output/'LICENSE.txt')
    if args.zip:
        args.zip.parent.mkdir(parents=True,exist_ok=True)
        with zipfile.ZipFile(args.zip,'w',zipfile.ZIP_DEFLATED) as z:
            for path in sorted(args.output.rglob('*')):
                if path.is_file() and not path.name.startswith('_probe-'):
                    z.write(path,Path('Hex-MkI-Sounds-Vol1')/path.relative_to(args.output))
        print(args.zip)
    print(f'Encoded {len(voices)} distinct single-voice DX7 sounds in {args.output}')

if __name__=='__main__': main()

"""Check real wizard/dialog, startup action, and recovery across processes."""
import argparse
import json
import os
from pathlib import Path
import subprocess


def suite(tool, launcher, output):
    output.mkdir(parents=True, exist_ok=False)
    env = dict(os.environ, PAINENGINE_ENGINE_STORE=str(output / 'EngineStore'),
               TEMP=str(output), TMP=str(output), PAIN_EDITOR_TEST_MODE='1')
    # Windows PowerShell must discover its own modules when the runner is
    # launched from PowerShell 7 (same environment rule as production CodeTools).
    env = {key: value for key, value in env.items() if key.upper() != 'PSMODULEPATH'}
    project = output / 'Project'
    subprocess.run([str(launcher), '--create-project', str(project), 'Authoring'],
                   env=env, check=True, capture_output=True, timeout=120)
    manifest = project / 'Project.json'
    cases = []

    def run(name, command, request):
        inp, result = output / f'{name}.request.json', output / f'{name}.result.json'
        inp.write_text(json.dumps(request), encoding='utf-8')
        process = subprocess.run([str(tool), command, str(inp), str(result)],
                                 env=env, capture_output=True, timeout=90)
        (output / f'{name}.log').write_bytes(process.stdout + process.stderr)
        data = json.loads(result.read_text(encoding='utf-8-sig'))
        assert process.returncode == 0 and data['ok'], (name, process.returncode, data)
        cases.append(name)
        return data

    wizard = run('wizard', 'ui.class-wizard', dict(projectRoot=str(project), steps=[
        dict(name='class'), dict(name='1Bad'), dict(name='RoundHero', kind='Character'),
        dict(name='RoundRules', kind='Object')], image=str(output / 'wizard.png')))
    assert [s['accepted'] for s in wizard['steps']] == [False, False, True, True]
    assert 'Engine/Components/Character.h' in wizard['steps'][2]['arguments']
    assert '-Base' not in wizard['steps'][3]['arguments']
    source = output / 'DialogModel.obj'
    source.write_text('v -1 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n', encoding='utf-8')
    request = dict(project=str(manifest), source=str(source))
    canceled = run('dialog-cancel', 'project.import', dict(request, dialog=dict(
        useFileUnitScale=False, presets=['Preset001', 'Preset1'], action='cancel', image=str(output / 'cancel.png'))))
    assert not canceled['accepted'] and canceled['before'] == canceled['after']
    assert not list((project / 'Content').rglob('DialogModel*'))
    imported = run('dialog-presets', 'project.import', dict(request, dialog=dict(
        useFileUnitScale=False, presets=['Preset1', 'Preset01', 'Preset001'], action='accept', image=str(output / 'presets.png'))))
    assert imported['accepted'] and imported['sidecar']['version'] == 2
    assert abs(imported['sidecar']['model']['scale'] - .01) < 1e-7
    assert [x['scale'] for x in imported['dialog']['presets']] == [1, .1, .01]
    assert imported['dialog']['spinFocused']
    placed = [o for o in imported['after'] if o['id'] not in {o['id'] for o in imported['before']}]
    assert len(placed) == 1 and placed[0]['scale'] == [1, 1, 1], placed
    scene = project / 'Content' / 'Scenes' / 'Continuation.scene.json'
    data = run('startup', 'project.scene', dict(project=str(manifest), editorWindow=True, steps=[
        dict(action='new'), dict(action='create', type='Primitive.Cube', name='Persisted'),
        dict(action='save', path=str(scene)), dict(action='startup')]))
    assert all(step['status'] == 0 for step in data['steps']), data
    assert json.loads(manifest.read_text(encoding='utf-8-sig'))['startupScene'].replace('\\', '/') == 'Content/Scenes/Continuation.scene.json'
    invalid = run('invalid-property-ui', 'project.scene', dict(project=str(manifest), editorWindow=True, steps=[
        dict(action='property', target='Persisted', path='/missing-property', value=12),
        dict(action='save', path=str(scene)), dict(action='startup')]))
    assert invalid['steps'][0]['status'] != 0 and invalid['steps'][0]['error']
    assert all(step['status'] == 0 for step in invalid['steps'][1:]) and invalid['dialogs']
    first = run('recovery-write', 'project.scene', dict(project=str(manifest), recovery=True, steps=[
        dict(action='create', type='Primitive.Cube', name='Recovered'), dict(action='write-recovery')]))
    assert first['recoveryRecords'] and not first['recoveryCandidates'], first
    # End without RequestExit: the real host leaves an abandoned checkpoint.
    second = run('recovery-load', 'project.scene', dict(project=str(manifest), recovery=True,
        steps=[dict(action='recover')]))
    assert second['steps'][0]['status'] == 0
    assert {'Persisted', 'Recovered'} <= {obj['name'] for obj in second['steps'][0]['objects']}
    assert not second['recoveryCandidates'], second
    # The same production menu action requires an enabled C++ project.
    enabled = subprocess.run(['powershell.exe', '-NoProfile', '-ExecutionPolicy', 'Bypass',
        '-File', str(tool.parent / 'SDK' / 'ProjectTools.ps1'), '-Action', 'Enable',
        '-Project', str(manifest), '-Runtime', str(tool.parent)], env=env,
        capture_output=True, timeout=240)
    (output / 'enable-code.log').write_bytes(enabled.stdout + enabled.stderr)
    assert enabled.returncode == 0, 'C++ fixture enable failed; see enable-code.log'
    generated = run('wizard-generate', 'project.class-wizard', dict(
        project=str(manifest), name='WizardProbe', kind='Object'))
    assert generated['wizardAccepted'] and 'class WizardProbe' in generated['generated']['.h']
    assert (project / 'Source/WizardProbe.cpp').is_file()
    duplicate = run('wizard-duplicate', 'project.class-wizard', dict(
        project=str(manifest), name='WizardProbe', kind='Object'))
    assert not duplicate['wizardAccepted'] and duplicate['wizardError'] and not duplicate['generated']
    summary = dict(passed=len(cases), cases=cases, visual='Actual themed Qt widgets; PNGs exclude native D3D viewport.')
    (output / 'summary.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
    print(json.dumps(summary))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ['tool', 'launcher', 'output']:
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    suite(args.tool.resolve(strict=True), args.launcher.resolve(strict=True), args.output.resolve())

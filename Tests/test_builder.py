# SPDX-License-Identifier: AGPL-3.0-or-later
# Windows discovery/Unicode regression fixtures adapted from Buffkiller 1.0.0.
"""Regression tests for juceaide's Windows path handling (fix3).

Filesystem/copy operations are real in temp folders. The juceaide/rc subprocess
calls are simulated here; the builder runs their real Windows binaries before
building the plugin. No Windows installation or DAW test is claimed by these tests.
"""
import importlib.util
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('flysnapper_unicode_fix', ROOT/'build_plugin.py')
b = importlib.util.module_from_spec(spec)
spec.loader.exec_module(b)

# Exact relevant snippets from pinned JUCE Main.cpp. This is a patch fixture,
# not a replacement JUCE implementation or a claim that JUCE is compiled here.
ORIGINAL = ('/* upstream license stays unchanged */\n'
    + b._JUCE_MAIN_BEFORE + '\n'
    + b._JUCE_ARGUMENTS_BEFORE + '\n'
    + b._JUCE_DISPATCH_BEFORE + '\n}\n')


def write(path, data=b'fixture'):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)
    return path


def source_fixture(root):
    write(root/b.JUCEAIDE_MAIN, ORIGINAL.encode('utf-8'))
    write(root/'CMakeLists.txt', b'# upstream source, unchanged\n')
    write(root/'LICENSE.md', b'upstream licence')
    write(root/'modules/unchanged.h', b'// untouched DSP/framework\n')
    write(root/'.git/HEAD', b'pinned commit fixture\n')
    return root


def tools_fixture(root):
    return b.CheckedTools({'PATH': 'fixture'}, root/'MSVC',
        b.WindowsSdk(root/'SDK', '10.0.26100.0', root/'SDK/bin/x64'),
        'Ninja', root/'ninja.exe')


def helper_fixture(root):
    build=root/'build'; tools=tools_fixture(root)
    helper=write(build/'juce/tools/extras/Build/juceaide/juceaide_artefacts/Custom/juceaide.exe')
    info=write(build/'Flysnapper_artefacts/JuceLibraryCode/Info.txt', b'PROJECT_NAME\x1fFlysnapper\x1eVERSION\x1f0.1.0\x1e')
    return build, tools, helper, info


class PatchTests(unittest.TestCase):
    def test_wide_entry_and_direct_wide_string_on_windows(self):
        result=b.patched_juceaide_source(ORIGINAL)
        self.assertIn('#if JUCE_WINDOWS\nint wmain (int argc, wchar_t** argv)',result)
        self.assertIn('const wchar_t* text',result)
        self.assertIn('return juce::String (text);',result)
        self.assertTrue(result.startswith('/* upstream license stays unchanged */'))

    def test_non_windows_keeps_utf8_entry(self):
        result=b.patched_juceaide_source(ORIGINAL)
        self.assertIn('#else\nint main (int argc, char** argv)',result)
        self.assertIn('juce::CharPointer_UTF8 (text)',result)

    def test_preserves_descriptive_juce_file_failures(self):
        result=b.patched_juceaide_source(ORIGINAL)
        self.assertIn('return juce::ConsoleApplication::invokeCatchingFailures ([&]',result)
        self.assertEqual(result.count('return it->second (std::move (argumentList));'),1)

    def test_rejects_unknown_missing_or_duplicate_patterns(self):
        for altered in ('int main() {}', ORIGINAL+ORIGINAL, ORIGINAL.replace('char** argv','char* argv[]')):
            with self.subTest(altered=altered[:30]), self.assertRaises(RuntimeError):
                b.patched_juceaide_source(altered)

    def test_crlf_input_is_supported(self):
        self.assertEqual(b.patched_juceaide_source(ORIGINAL),
                         b.patched_juceaide_source(ORIGINAL.replace('\n','\r\n')))

    def test_refuses_double_patching(self):
        with self.assertRaises(RuntimeError):
            b.patched_juceaide_source(b.patched_juceaide_source(ORIGINAL))

    def test_source_payload_is_not_silently_reinterpreted(self):
        filename='C:/Users/Olav Lægreid/Flysnapper/Info.txt'
        with self.assertRaises(UnicodeDecodeError):
            filename.encode('cp1252').decode('utf-8')
        self.assertEqual(filename.encode('utf-16-le').decode('utf-16-le'),filename)


class SnapshotTests(unittest.TestCase):
    def test_copy_changes_only_juceaide_and_keeps_original_clean(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)/'Olav Lægreid';source=source_fixture(root/'deps/JUCE')
            original=b.source_file_hashes(source)
            with mock.patch.object(b,'ROOT',root):
                copy=b.prepare_unicode_juce(source)
            self.assertEqual(b.source_file_hashes(source),original)
            self.assertTrue((source/'.git/HEAD').is_file())
            self.assertFalse((copy/'.git').exists())
            actual=b.source_file_hashes(copy)
            self.assertEqual(set(actual),set(original))
            self.assertEqual([k for k in original if original[k]!=actual[k]],[b.JUCEAIDE_MAIN])
            manifest=json.loads((copy/b.JUCEAIDE_PATCH_MANIFEST).read_text())
            self.assertEqual(manifest['upstream_commit'],b.JUCE_COMMIT)
            self.assertEqual(manifest['patch'],b.JUCEAIDE_PATCH_REVISION)

    def test_repeat_uses_verified_copy_without_rewriting(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp); source=source_fixture(root/'deps/JUCE')
            with mock.patch.object(b,'ROOT',root):
                copy=b.prepare_unicode_juce(source)
                timestamp=(copy/b.JUCEAIDE_MAIN).stat().st_mtime_ns
                with mock.patch.object(b.shutil,'copytree',side_effect=AssertionError('unnecessary copy')):
                    self.assertEqual(b.prepare_unicode_juce(source),copy)
                self.assertEqual((copy/b.JUCEAIDE_MAIN).stat().st_mtime_ns,timestamp)

    def test_modified_build_copy_is_not_trusted_or_overwritten(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);source=source_fixture(root/'deps/JUCE')
            with mock.patch.object(b,'ROOT',root):
                copy=b.prepare_unicode_juce(source)
                changed=copy/'modules/unchanged.h';changed.write_text('external edit')
                with self.assertRaises(RuntimeError):b.prepare_unicode_juce(source)
                self.assertEqual(changed.read_text(),'external edit')

    def test_missing_manifest_is_not_trusted(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);source=source_fixture(root/'deps/JUCE')
            with mock.patch.object(b,'ROOT',root):
                copy=b.prepare_unicode_juce(source)
                (copy/b.JUCEAIDE_PATCH_MANIFEST).unlink()
                with self.assertRaises(RuntimeError):b.prepare_unicode_juce(source)

    def test_different_source_gets_separate_copy(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);source=source_fixture(root/'deps/JUCE')
            with mock.patch.object(b,'ROOT',root):
                first=b.prepare_unicode_juce(source)
                (source/'modules/unchanged.h').write_text('new fixture source')
                second=b.prepare_unicode_juce(source)
            self.assertNotEqual(first,second)
            self.assertTrue(first.is_dir())

    def test_incomplete_source_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);source=source_fixture(root/'deps/JUCE')
            (source/'LICENSE.md').unlink()
            with mock.patch.object(b,'ROOT',root),self.assertRaises(RuntimeError):b.prepare_unicode_juce(source)

    def test_partial_copy_failure_is_not_published(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);source=source_fixture(root/'deps/JUCE')
            with mock.patch.object(b,'ROOT',root),mock.patch.object(b.shutil,'copytree',side_effect=OSError('full disk')):
                with self.assertRaises(OSError):b.prepare_unicode_juce(source)
            self.assertEqual(list((root/'build/j').iterdir()),[])
            self.assertEqual((source/b.JUCEAIDE_MAIN).read_text(),ORIGINAL)

    @unittest.skipIf(os.name=='nt','symlink creation can require privilege on Windows')
    def test_external_symlinks_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);source=source_fixture(root/'deps/JUCE')
            (source/'external').symlink_to(root,target_is_directory=True)
            with self.assertRaises(RuntimeError):b.source_file_hashes(source)


class ResourceProbeTests(unittest.TestCase):
    def test_actual_output_required_and_unicode_arguments_kept(self):
        with tempfile.TemporaryDirectory() as temp:
            build,tools,helper,info=helper_fixture(Path(temp)/'Olav Lægreid')
            calls=[]
            def run(args,**kwargs):
                calls.append(args)
                self.assertEqual(kwargs['env'],tools.environment)
                if args[0]==helper:
                    self.assertIn('æøå',str(args[2])); self.assertIn('æøå',str(args[3]))
                    self.assertEqual(Path(args[2]).read_bytes(),info.read_bytes())
                    Path(args[3]).write_bytes(b'1 VERSIONINFO // Flysnapper\n')
                else:
                    self.assertEqual(args[0],tools.sdk.bin_dir/'rc.exe')
                    self.assertEqual(args[1:3],['/nologo','/fo'])
                    Path(args[3]).write_bytes(b'compiled resource fixture')
                return ''
            with mock.patch.object(b,'run',side_effect=run):b.preflight_juceaide_unicode(build,tools)
            self.assertEqual(len(calls),2)

    def test_missing_helper_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            build,tools,helper,_=helper_fixture(Path(temp));helper.unlink()
            with self.assertRaises(RuntimeError):b.preflight_juceaide_unicode(build,tools)

    def test_missing_info_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            build,tools,_,info=helper_fixture(Path(temp));info.unlink()
            with self.assertRaises(RuntimeError):b.preflight_juceaide_unicode(build,tools)

    def test_old_resource_does_not_mask_new_failure(self):
        with tempfile.TemporaryDirectory() as temp:
            build,tools,_,_=helper_fixture(Path(temp))
            output=write(build/'juceaide-unicode-check/Olav Lægreid/Flysnapper æøå.rc', b'1 VERSIONINFO Flysnapper')
            with mock.patch.object(b,'run',return_value=''),self.assertRaises(RuntimeError):
                b.preflight_juceaide_unicode(build,tools)
            self.assertFalse(output.exists())

    def test_helper_failure_stops_before_rc(self):
        with tempfile.TemporaryDirectory() as temp:
            build,tools,_,_=helper_fixture(Path(temp))
            with mock.patch.object(b,'run',side_effect=b.CommandError(['helper'],1,'failure')) as run, self.assertRaises(b.CommandError):
                b.preflight_juceaide_unicode(build,tools)
            self.assertEqual(run.call_count,1)

    def test_empty_or_invalid_resource_rejected(self):
        for contents in (b'',b'unrelated file',b'VERSIONINFO not our project'):
            with self.subTest(contents=contents),tempfile.TemporaryDirectory() as temp:
                build,tools,_,_=helper_fixture(Path(temp))
                def run(args,**kwargs):Path(args[3]).write_bytes(contents);return ''
                with mock.patch.object(b,'run',side_effect=run) as calls,self.assertRaises(RuntimeError):
                    b.preflight_juceaide_unicode(build,tools)
                self.assertEqual(calls.call_count,1)

    def test_resource_compiler_must_create_nonempty_output(self):
        with tempfile.TemporaryDirectory() as temp:
            build,tools,helper,_=helper_fixture(Path(temp))
            def run(args,**kwargs):
                if args[0]==helper:Path(args[3]).write_bytes(b'1 VERSIONINFO Flysnapper')
                return ''
            with mock.patch.object(b,'run',side_effect=run),self.assertRaises(RuntimeError):
                b.preflight_juceaide_unicode(build,tools)

    def test_resource_compiler_failure_is_propagated(self):
        with tempfile.TemporaryDirectory() as temp:
            build,tools,helper,_=helper_fixture(Path(temp))
            def run(args,**kwargs):
                if args[0]==helper:Path(args[3]).write_bytes(b'1 VERSIONINFO Flysnapper');return ''
                raise b.CommandError(['rc'],1,'RC compilation failed')
            with mock.patch.object(b,'run',side_effect=run),self.assertRaises(b.CommandError):
                b.preflight_juceaide_unicode(build,tools)

    def test_new_cmake_cache_leaves_old_cache_unchanged(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);tools=tools_fixture(root)
            with mock.patch.object(b,'ROOT',root):
                old=b.checked_build_directory(tools,'direct')
                cache=write(old/'CMakeCache.txt',b'old failed build')
                with mock.patch.object(b,'run',return_value='') as run:
                    new=b.configure_checked_project('cmake',root/'unicode-JUCE',tools)
                self.assertNotEqual(old,new)
                self.assertTrue(new.name.startswith('w-'))
                self.assertEqual(len(new.name),14)
                self.assertIn('-DCMAKE_OBJECT_PATH_MAX=220',run.call_args.args[0])
                self.assertEqual(cache.read_bytes(),b'old failed build')
                self.assertIn('-DJUCE_DIR='+str(root/'unicode-JUCE'),run.call_args.args[0])




"""Windows SDK discovery regression tests. Windows execution is simulated.

No test installs an SDK, downloads software, or edits Windows settings. The
builder's real preflight compile/run executes only on the user's Windows host.
"""
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('flysnapper_sdk_fix', ROOT / 'build_plugin.py')
b = importlib.util.module_from_spec(spec)
spec.loader.exec_module(b)


def touch(path):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.touch()
    return path


def sdk_files(root, version='10.0.26100.0', arch='x64'):
    for name in b.SDK_REQUIRED_FILES:
        touch(root / name.format(version=version))
    binary = root / 'bin' / version / arch
    touch(binary / 'rc.exe')
    touch(binary / 'mt.exe')
    return b.WindowsSdk(root.resolve(), version, binary.resolve())


def msvc_files(installation, version='14.51.36231'):
    msvc = installation / 'VC/Tools/MSVC' / version
    for name in b.MSVC_REQUIRED_FILES:
        touch(msvc / name)
    touch(msvc / 'bin/Hostx64/x64/nmake.exe')
    return msvc.resolve()


def checked_tools(root):
    install = root / 'Olav Lægreid' / 'Build Tools'
    sdk = sdk_files(root / 'Windows Kits/10')
    msvc = msvc_files(install)
    env = b.explicit_build_environment({'PATH': 'original'}, msvc, sdk)
    return b.CheckedTools(env, msvc, sdk, 'NMake Makefiles', msvc/'bin/Hostx64/x64/nmake.exe')


class SdkDiscoveryTests(unittest.TestCase):
    def test_sdk_found_when_rc_not_on_path(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / 'Windows Kits/10'
            expected = sdk_files(root)
            with mock.patch.object(b, 'sdk_roots', return_value=[root]):
                self.assertEqual(b.find_windows_sdk({'PATH': 'no SDK'}), expected)

    def test_absent_sdk_reports_all_checked_locations_and_install_option(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / 'empty'
            with mock.patch.object(b, 'sdk_roots', return_value=[root]), self.assertRaises(b.SdkNotFoundError) as caught:
                b.find_windows_sdk({})
            self.assertIn(str(root), str(caught.exception))
            self.assertIn('--install-sdk', str(caught.exception))

    def test_missing_rc_or_mt_is_rejected(self):
        for missing in ('rc.exe', 'mt.exe'):
            with self.subTest(missing=missing), tempfile.TemporaryDirectory() as tmp:
                sdk = sdk_files(Path(tmp))
                (sdk.bin_dir / missing).unlink()
                result, missing_files = b.inspect_sdk(sdk.root, sdk.version)
                self.assertIsNone(result)
                self.assertTrue(any('rc.exe OG mt.exe' in s for s in missing_files))

    def test_tools_without_headers_are_not_a_complete_sdk(self):
        with tempfile.TemporaryDirectory() as tmp:
            sdk = sdk_files(Path(tmp))
            (sdk.root/'Include'/sdk.version/'um/Windows.h').unlink()
            result, missing = b.inspect_sdk(sdk.root, sdk.version)
            self.assertIsNone(result)
            self.assertIn('Include/'+sdk.version+'/um/Windows.h', missing)

    def test_ucrt_static_library_is_required(self):
        with tempfile.TemporaryDirectory() as tmp:
            sdk = sdk_files(Path(tmp))
            (sdk.root/'Lib'/sdk.version/'ucrt/x64/libucrt.lib').unlink()
            self.assertIsNone(b.inspect_sdk(sdk.root, sdk.version)[0])

    def test_complete_older_sdk_beats_partial_newer_one(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            expected = sdk_files(root, '10.0.26100.0')
            newer = sdk_files(root, '10.0.28000.0')
            (newer.bin_dir/'rc.exe').unlink()
            with mock.patch.object(b, 'sdk_roots', return_value=[root]):
                self.assertEqual(b.find_windows_sdk({}), expected)

    def test_numeric_version_order(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            sdk_files(root, '10.0.9900.0')
            newer = sdk_files(root, '10.0.26100.0')
            with mock.patch.object(b, 'sdk_roots', return_value=[root]):
                self.assertEqual(b.find_windows_sdk({}), newer)

    def test_never_combines_incomplete_sdk_versions(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            first = sdk_files(root, '10.0.26100.0')
            second = sdk_files(root, '10.0.28000.0')
            (first.bin_dir/'mt.exe').unlink()
            (second.root/'Include'/second.version/'um/Windows.h').unlink()
            with mock.patch.object(b, 'sdk_roots', return_value=[root]), self.assertRaises(b.SdkNotFoundError):
                b.find_windows_sdk({})

    def test_x86_resource_tools_are_supported_with_x64_libraries(self):
        with tempfile.TemporaryDirectory() as tmp:
            sdk = sdk_files(Path(tmp), arch='x86')
            self.assertEqual(b.inspect_sdk(sdk.root, sdk.version)[0], sdk)

    def test_environment_registry_default_and_path_roots_are_considered(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            env = {'WindowsSdkDir': str(root/'env sdk'), 'ProgramFiles(x86)': str(root/'pf x86'),
                   'PATH': str(root/'path sdk/bin/10.0.26100.0/x64')}
            with mock.patch.object(b, 'registry_sdk_roots', return_value=[root/'registry sdk']):
                paths = b.sdk_roots(env)
            for p in ('env sdk', 'pf x86/Windows Kits/10', 'path sdk', 'registry sdk'):
                self.assertIn(root/p, paths)

    def test_no_recursive_scan_of_entire_drive(self):
        with mock.patch.object(b.Path, 'rglob', side_effect=AssertionError('full disk scan')), \
             mock.patch.object(b, 'registry_sdk_roots', return_value=[]):
            self.assertTrue(b.sdk_roots({}))

    def test_latest_complete_msvc_selected(self):
        with tempfile.TemporaryDirectory() as tmp:
            install = Path(tmp)
            expected = msvc_files(install, '14.51.36231')
            newer = msvc_files(install, '14.52.10000')
            (newer/'lib/x64/libcmt.lib').unlink()
            self.assertEqual(b.find_complete_msvc(install), expected)

    def test_compiler_exe_alone_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            install = Path(tmp)
            touch(install/'VC/Tools/MSVC/14.51.36231/bin/Hostx64/x64/cl.exe')
            with self.assertRaises(RuntimeError) as caught:
                b.find_complete_msvc(install)
            self.assertIn('libcmt.lib', str(caught.exception))


class EnvironmentFixTests(unittest.TestCase):
    def test_correct_child_environment_does_not_modify_parent(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            msvc=msvc_files(root/'BuildTools')
            sdk=sdk_files(root/'Olav Lægreid/Windows Kits/10')
            base={'Path':'previous', 'INCLUDE':'old SDK', 'LIB':'other arch', 'CMAKE_GENERATOR_INSTANCE':'bad',
                  'CL':'unwanted options', 'PRIVATE_TOKEN':'not logged'}
            saved=dict(base)
            env=b.explicit_build_environment(base,msvc,sdk)
            self.assertEqual(base,saved)
            self.assertIn(str(sdk.bin_dir),env['PATH'])
            self.assertTrue(env['PATH'].startswith(str(msvc/'bin/Hostx64/x64')+';'))
            self.assertNotIn('old SDK',env['INCLUDE'])
            self.assertNotIn('other arch',env['LIB'])
            self.assertNotIn('CMAKE_GENERATOR_INSTANCE',env)
            self.assertNotIn('CL',env)
            self.assertEqual(env['PRIVATE_TOKEN'],'not logged')
            self.assertEqual(env['VSCMD_ARG_TGT_ARCH'],'x64')

    def test_setup_recovers_after_exact_reported_rc_error(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            install=root/'BuildTools'
            msvc=msvc_files(install)
            sdk=sdk_files(root/'Windows Kits/10')
            with mock.patch.object(b,'developer_environment',side_effect=RuntimeError('VsDevCmd.bat: Windows SDK-verktøyet rc.exe er ikke tilgjengelig i byggemiljoet.')), \
                 mock.patch.object(b,'sdk_roots',return_value=[sdk.root]), \
                 mock.patch.object(b,'install_windows_sdk') as install_sdk:
                tools=b.prepare_checked_tools(install,['NMake Makefiles'],False)
            self.assertEqual(tools.sdk,sdk)
            self.assertEqual(tools.msvc,msvc)
            self.assertEqual(tools.generator,'NMake Makefiles')
            install_sdk.assert_not_called()

    def test_installed_ninja_preferred(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);install=root/'BuildTools'
            msvc_files(install);sdk=sdk_files(root/'sdk')
            ninja=touch(install/'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe')
            with mock.patch.object(b,'developer_environment',return_value={}), \
                 mock.patch.object(b,'sdk_roots',return_value=[sdk.root]):
                tools=b.prepare_checked_tools(install,['NMake Makefiles','Ninja'],False)
            self.assertEqual(tools.generator,'Ninja')
            self.assertEqual(tools.make_program,ninja.resolve())

    def test_explicit_rc_and_mt_forwarded_to_cmake(self):
        with tempfile.TemporaryDirectory() as tmp:
            tools=checked_tools(Path(tmp))
            flags=b.checked_cmake_flags(tools)
            self.assertIn('-DCMAKE_RC_COMPILER:FILEPATH='+(tools.sdk.bin_dir/'rc.exe').as_posix(),flags)
            self.assertIn('-DCMAKE_MT:FILEPATH='+(tools.sdk.bin_dir/'mt.exe').as_posix(),flags)
            self.assertIn('-DCMAKE_LINKER:FILEPATH='+(tools.msvc/'bin/Hostx64/x64/link.exe').as_posix(),flags)
            self.assertNotIn('-A',flags)
            self.assertFalse(any('GENERATOR_INSTANCE' in x for x in flags))

    def test_cache_changes_with_sdk_and_toolchain(self):
        with tempfile.TemporaryDirectory() as tmp:
            tools=checked_tools(Path(tmp))
            original=b.checked_build_directory(tools,'direct')
            other=tools._replace(sdk=tools.sdk._replace(version='10.0.28000.0'))
            self.assertNotEqual(original,b.checked_build_directory(other,'direct'))
            self.assertIn('fix4',str(original))

    def test_checked_project_never_uses_vs_instance_generator(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);tools=checked_tools(root)
            with mock.patch.object(b,'ROOT',root),mock.patch.object(b,'run',return_value='') as run:
                b.configure_checked_project('cmake',root/'deps/JUCE',tools)
            command=list(map(str,run.call_args.args[0]))
            self.assertEqual(command[command.index('-G')+1],'NMake Makefiles')
            self.assertFalse(any('GENERATOR_INSTANCE' in s for s in command))
            self.assertEqual(run.call_args.kwargs['env'],tools.environment)


class InstallSafetyTests(unittest.TestCase):
    def test_never_installs_when_sdk_exists_even_with_permission_flag(self):
        sdk=b.WindowsSdk(Path('sdk'),'10.0.26100.0',Path('sdk/bin/x64'))
        with mock.patch.object(b,'find_windows_sdk',return_value=sdk), \
             mock.patch.object(b,'install_windows_sdk') as install:
            self.assertEqual(b.ensure_windows_sdk({},True),sdk)
        install.assert_not_called()

    def test_no_download_or_install_without_explicit_flag(self):
        with mock.patch.object(b,'find_windows_sdk',side_effect=b.SdkNotFoundError('missing')), \
             mock.patch.object(b,'install_windows_sdk') as install, self.assertRaises(b.SdkNotFoundError):
            b.ensure_windows_sdk({},False)
        install.assert_not_called()

    def test_rechecks_files_after_installer_and_calls_only_once(self):
        sdk=b.WindowsSdk(Path('sdk'),'10.0.26100.0',Path('sdk/bin/x64'))
        with mock.patch.object(b,'find_windows_sdk',side_effect=[b.SdkNotFoundError('missing'),sdk]) as find, \
             mock.patch.object(b,'install_windows_sdk') as install:
            self.assertEqual(b.ensure_windows_sdk({},True),sdk)
        install.assert_called_once();self.assertEqual(find.call_count,2)

    def test_success_exit_does_not_hide_still_missing_sdk(self):
        with mock.patch.object(b,'find_windows_sdk',side_effect=b.SdkNotFoundError('missing')), \
             mock.patch.object(b,'install_windows_sdk') as install, self.assertRaises(b.SdkNotFoundError):
            b.ensure_windows_sdk({},True)
        install.assert_called_once()

    def test_only_https_microsoft_download_host_allowed(self):
        self.assertTrue(b.trusted_sdk_url(b.SDK_DOWNLOAD_URL))
        for url in ('http://download.microsoft.com/foo.exe','https://download.microsoft.com.evil/foo.exe',
                    'https://example.com/file.exe','file:///test','https://user@download.microsoft.com/x.exe'):
            self.assertFalse(b.trusted_sdk_url(url),url)

    def test_redirect_to_other_host_rejected_before_following(self):
        with self.assertRaises(RuntimeError):
            b.MicrosoftSdkRedirect().redirect_request(None,None,302,'Found',{},'https://example.com/evil.exe')

    def test_signature_command_requires_valid_microsoft_publisher(self):
        with mock.patch.object(b,'powershell_executable',return_value=Path('powershell.exe')), \
             mock.patch.object(b,'run',return_value='FLYSNAPPER_MICROSOFT_SIGNATURE_OK') as run:
            b.verify_microsoft_signature(Path('Olav Lægreid/test.exe'))
        command=run.call_args.args[0][-1]
        self.assertIn("$s.Status -ne 'Valid'",command)
        self.assertIn("-ne 'Microsoft Corporation'",command)
        self.assertEqual(run.call_args.kwargs['env']['FLYSNAPPER_SDK_SETUP'],str(Path('Olav Lægreid/test.exe')))
        self.assertNotIn('-ExecutionPolicy',run.call_args.args[0])

    def test_signature_check_requires_success_marker(self):
        with mock.patch.object(b,'powershell_executable',return_value=Path('powershell.exe')), \
             mock.patch.object(b,'run',return_value='signature not verified'), self.assertRaises(RuntimeError):
            b.verify_microsoft_signature(Path('setup.exe'))

    def test_installer_only_selects_x64_and_never_reboots(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            with mock.patch.object(b,'LOG_DIR',root), \
                 mock.patch.object(b,'download_sdk_installer',return_value=root/'setup.exe'), \
                 mock.patch.object(b,'powershell_executable',return_value=Path('powershell.exe')), \
                 mock.patch.object(b,'run',return_value='FLYSNAPPER_SDK_EXIT=0') as run:
                b.install_windows_sdk()
            env=run.call_args.kwargs['env']
            args=env['FLYSNAPPER_SDK_ARGS']
            self.assertIn('/norestart',args)
            self.assertIn('OptionId.DesktopCPPx64',args)
            self.assertNotIn('/repair',args)
            self.assertIn('-Verb RunAs',run.call_args.args[0][-1])
            self.assertIn('-Wait',run.call_args.args[0][-1])

    def test_installer_exit_failure_or_restart_never_reported_as_ready(self):
        for code in (3010,1602,1603):
            with self.subTest(code=code),tempfile.TemporaryDirectory() as tmp:
                with mock.patch.object(b,'LOG_DIR',Path(tmp)), \
                     mock.patch.object(b,'download_sdk_installer',return_value=Path(tmp)/'setup.exe'), \
                     mock.patch.object(b,'powershell_executable',return_value=Path('powershell.exe')), \
                     mock.patch.object(b,'run',return_value=f'FLYSNAPPER_SDK_EXIT={code}'), self.assertRaises(RuntimeError):
                    b.install_windows_sdk()


class PreflightTests(unittest.TestCase):
    def test_requires_actual_executable_and_success_output(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);tools=checked_tools(root)
            with mock.patch.object(b,'ROOT',root):
                expected=b.checked_build_directory(tools,'sdk-check')
                def run(args,**kwargs):
                    self.assertEqual(kwargs['env'],tools.environment)
                    if '--build' in args:
                        touch(expected/'FlysnapperToolchainCheck.exe')
                    if str(args[0]).endswith('FlysnapperToolchainCheck.exe'):
                        return 'FLYSNAPPER_TOOLCHAIN_OK'
                    return ''
                with mock.patch.object(b,'run',side_effect=run) as executed:
                    b.preflight_toolchain('cmake',tools)
                self.assertEqual(executed.call_count,3)
                self.assertIn('RC',(expected/'source/CMakeLists.txt').read_text())
                self.assertIn('LoadStringW',(expected/'source/probe.cpp').read_text())
                self.assertIn('STRINGTABLE',(expected/'source/probe.rc').read_text())

    def test_compile_failure_does_not_run_the_test_executable(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);tools=checked_tools(root)
            with mock.patch.object(b,'ROOT',root), \
                 mock.patch.object(b,'run',side_effect=b.CommandError(['cmake'],1,'compiler failed')) as run, \
                 self.assertRaises(b.CommandError):
                b.preflight_toolchain('cmake',tools)
            self.assertEqual(run.call_count,1)

    def test_no_false_positive_if_build_created_no_executable(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);tools=checked_tools(root)
            with mock.patch.object(b,'ROOT',root),mock.patch.object(b,'run',return_value='') as run, \
                 self.assertRaises(RuntimeError):
                b.preflight_toolchain('cmake',tools)
            self.assertEqual(run.call_count,2)

    def test_no_false_positive_if_executable_returns_wrong_output(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);tools=checked_tools(root)
            with mock.patch.object(b,'ROOT',root):
                touch(b.checked_build_directory(tools,'sdk-check')/'FlysnapperToolchainCheck.exe')
                with mock.patch.object(b,'run',return_value='not valid'),self.assertRaises(RuntimeError):
                    b.preflight_toolchain('cmake',tools)



class FlysnapperSafetyTests(unittest.TestCase):
    def make_windows_dist(self, root):
        dist=root/'dist'
        binary=dist/'Flysnapper.vst3/Contents/x86_64-win/Flysnapper.vst3'
        binary.parent.mkdir(parents=True)
        data=bytearray(512);data[:2]=b'MZ';data[60:64]=(128).to_bytes(4,'little');data[128:134]=b'PE\0\0\x64\x86'
        binary.write_bytes(data)
        manifest={'platform':'Windows-x64','tests':'passed','vst3_sha256':b.hashlib.sha256(data).hexdigest(),
                  'bundle_sha256':b.bundle_hashes(dist/'Flysnapper.vst3')}
        (dist/'build-manifest.json').write_text(json.dumps(manifest))
        return dist,binary,manifest

    def test_default_does_not_install(self):
        args=b.argument_parser().parse_args([])
        self.assertFalse(args.install)
        self.assertFalse(args.install_only)

    def test_invalid_destination_does_not_implicitly_install(self):
        with self.assertRaises(SystemExit): b.main(['--install-dir','relative'])

    def test_native_cannot_install_windows_bundle(self):
        with self.assertRaises(SystemExit): b.main(['--native','--install'])

    def test_replaces_only_flysnapper_and_keeps_recovery_outside_scan(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);dist,binary,_=self.make_windows_dist(root);destination=root/'Plugins'
            old=write(destination/'Flysnapper.vst3/old.txt',b'old Flysnapper')
            other=write(destination/'Buffkiller.vst3/keep.txt',b'unchanged Buffkiller')
            with mock.patch.object(b,'DIST',dist),mock.patch.object(b,'ROOT',root/'Source'): b.install_bundle(destination)
            self.assertFalse(old.exists());self.assertEqual(other.read_bytes(),b'unchanged Buffkiller')
            self.assertEqual((destination/'Flysnapper.vst3/Contents/x86_64-win/Flysnapper.vst3').read_bytes(),binary.read_bytes())
            backups=list((dist/'previous-installations').glob('*/Flysnapper.vst3/old.txt'))
            self.assertEqual(len(backups),1);self.assertEqual(backups[0].read_bytes(),b'old Flysnapper')
            self.assertEqual(sorted(p.name for p in destination.iterdir()),['Buffkiller.vst3','Flysnapper.vst3'])

    def test_copy_failure_restores_old_installation(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);dist,_,_=self.make_windows_dist(root);destination=root/'Plugins'
            old=write(destination/'Flysnapper.vst3/old.txt',b'old')
            original_rename=Path.rename
            def fail_staging(path,target):
                if path.name=='Flysnapper.installing': raise OSError('simulated target rename failure')
                return original_rename(path,target)
            with mock.patch.object(b,'DIST',dist),mock.patch.object(b,'ROOT',root/'Source'),mock.patch.object(Path,'rename',fail_staging),self.assertRaises(OSError):
                b.install_bundle(destination)
            self.assertEqual(old.read_bytes(),b'old')

    def test_install_fixture_in_actual_builder_temporary_context(self):
        # main() gives its child unittest runner project-local TEMP/TMP/TMPDIR.
        # Fixture Source and Plugins must still be separate siblings there.
        with b.local_temporary_directory() as temporary_root:
            with tempfile.TemporaryDirectory() as temp:
                root=Path(temp)
                self.assertIn(temporary_root,root.parents)
                dist,_,_=self.make_windows_dist(root)
                source=root/'Source';source.mkdir()
                with mock.patch.object(b,'DIST',dist),mock.patch.object(b,'ROOT',source):
                    with self.assertRaises(RuntimeError):b.install_bundle(source/'Plugins')
                    b.install_bundle(root/'Plugins')
                self.assertTrue((root/'Plugins/Flysnapper.vst3/Contents/x86_64-win/Flysnapper.vst3').is_file())

    def test_changed_resource_or_binary_blocks_install(self):
        for item in ('Contents/x86_64-win/Flysnapper.vst3','Contents/Resources/extra.txt'):
            with self.subTest(item=item),tempfile.TemporaryDirectory() as temp:
                root=Path(temp);dist,_,_=self.make_windows_dist(root)
                write(dist/'Flysnapper.vst3'/item,b'tampered')
                with mock.patch.object(b,'DIST',dist),mock.patch.object(b,'ROOT',root/'Source'),self.assertRaises(RuntimeError):b.install_bundle(root/'Plugins')
                self.assertFalse((root/'Plugins').exists())

    def test_failed_tests_or_native_manifest_blocks_install(self):
        for key,value in (('tests','failed'),('platform','Linux-x86_64')):
            with self.subTest(key=key),tempfile.TemporaryDirectory() as temp:
                root=Path(temp);dist,_,manifest=self.make_windows_dist(root);manifest[key]=value
                (dist/'build-manifest.json').write_text(json.dumps(manifest))
                with mock.patch.object(b,'DIST',dist),mock.patch.object(b,'ROOT',root/'Source'),self.assertRaises(RuntimeError):b.install_bundle(root/'Plugins')

    def test_stale_staging_is_preserved(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);dist,_,_=self.make_windows_dist(root);destination=root/'Plugins'
            keep=write(destination/'Flysnapper.installing/local.txt',b'preserve me')
            with mock.patch.object(b,'DIST',dist),mock.patch.object(b,'ROOT',root/'Source'),self.assertRaises(RuntimeError):b.install_bundle(destination)
            self.assertEqual(keep.read_bytes(),b'preserve me')

    def test_test_inventory_must_contain_required_tests(self):
        with tempfile.TemporaryDirectory() as temp:
            with mock.patch.object(b,'run',return_value=json.dumps({'tests':[]})) as run,self.assertRaises(RuntimeError):
                b.run_required_tests('ctest',Path(temp))
            self.assertEqual(run.call_count,1)

    def test_missing_current_results_cannot_reuse_old_file(self):
        with tempfile.TemporaryDirectory() as temp:
            build=Path(temp);result=write(build/'flysnapper-ctest.xml',b'<testsuite/>')
            stale_frontend=[write(build/name,b'old frontend proof') for name in (b.input_dynamics_evidence.REPORT,b.input_dynamics_evidence.EVENTS)]
            inventory=json.dumps({'tests':[{'name':n} for n in b.REQUIRED_TESTS]})
            with mock.patch.object(b,'run',side_effect=[inventory,'']),self.assertRaises(RuntimeError):
                b.run_required_tests('ctest',build)
            self.assertFalse(result.exists())
            self.assertTrue(all(not path.exists() for path in stale_frontend))

    def test_skipped_required_test_fails_readiness(self):
        with tempfile.TemporaryDirectory() as temp:
            build=Path(temp);inventory=json.dumps({'tests':[{'name':n} for n in b.REQUIRED_TESTS]})
            def run(args,**kwargs):
                if '--show-only=json-v1' in args:return inventory
                xml='<testsuite>'+''.join('<testcase name="'+n+'"><skipped/></testcase>' for n in b.REQUIRED_TESTS)+'</testsuite>'
                (build/'flysnapper-ctest.xml').write_text(xml);return ''
            with mock.patch.object(b,'run',side_effect=run),self.assertRaises(RuntimeError):b.run_required_tests('ctest',build)

    def test_current_complete_result_passes(self):
        with tempfile.TemporaryDirectory() as temp:
            build=Path(temp);inventory=json.dumps({'tests':[{'name':n} for n in b.REQUIRED_TESTS]})
            def run(args,**kwargs):
                if '--show-only=json-v1' in args:return inventory
                xml='<testsuite>'+''.join('<testcase name="'+n+'" status="run"/>' for n in b.REQUIRED_TESTS)+'</testsuite>'
                (build/'flysnapper-ctest.xml').write_text(xml);return ''
            with mock.patch.object(b,'run',side_effect=run):b.run_required_tests('ctest',build)

    def test_ctest_failure_exit_propagates(self):
        with tempfile.TemporaryDirectory() as temp:
            inventory=json.dumps({'tests':[{'name':n} for n in b.REQUIRED_TESTS]})
            with mock.patch.object(b,'run',side_effect=[inventory,b.CommandError(['ctest'],8,'test failed')]),self.assertRaises(b.CommandError):
                b.run_required_tests('ctest',Path(temp))

    def test_build_command_contains_current_targets_only(self):
        cmd=b.build_command('cmake',Path('build'),'Ninja',2)
        for name in ('Flysnapper_VST3','FlysnapperCoreTests','FlysnapperPluginTests','FlysnapperVst3HostTests','FlysnapperGuitarQualityTests','FlysnapperBendProcessorTests',
                     'FlysnapperNoiseTests','FlysnapperInputDynamicsTests','FlysnapperDynamicsProcessorTests','FlysnapperMpeOutputTests','FlysnapperMpeProcessorTests','FlysnapperMpeVst3HostTests'):
            self.assertIn(name,cmd)
        self.assertFalse(any('Buffkiller' in s or 'RealtimeTests' in s for s in cmd))


if __name__=='__main__':unittest.main()

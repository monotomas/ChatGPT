# SPDX-License-Identifier: AGPL-3.0-or-later
"""Regressions for the actual 264-character Windows /Fo compiler failure.
The CMake probe tests object-path generation; it does not simulate an MSVC run.
"""
import importlib.util,json,os,re,shutil,subprocess,sys,tempfile,unittest
from pathlib import Path,PureWindowsPath
from unittest import mock
spec=importlib.util.spec_from_file_location('flysnapper_shortpath_builder',Path(__file__).resolve().parents[1]/'build_plugin.py')
b=importlib.util.module_from_spec(spec);spec.loader.exec_module(b)

class ObjectPathTests(unittest.TestCase):
    def tools(self,root):
        return b.CheckedTools({},root/'MSVC',b.WindowsSdk(root/'SDK','10.0.26100.0',root/'SDK/bin/x64'),'Ninja',root/'ninja.exe')
    def test_exact_reported_ci_object_crosses_max_path_and_new_namespace_fits(self):
        root=PureWindowsPath('D:/a/_temp/Olav Lægreid/Flysnapper')
        old=root/'build/x64-direct-fix4-49fe0dde4775-unicode3/CMakeFiles/FlysnapperProcessorAudioTests.dir/build/dependencies/JUCE-unicode3-cf9b7be454ab0ea072d7/modules/juce_audio_processors_headless/juce_audio_processors_headless_lv2_libs.cpp.obj'
        new=root/'build/w-0123456789ab/CMakeFiles/FlysnapperProcessorAudioTests.dir/build/j/u-cf9b7be454ab0ea072d7/modules/juce_audio_processors_headless/juce_audio_processors_headless_lv2_libs.cpp.obj'
        self.assertEqual(len(str(old)),264)
        self.assertLess(len(str(new)),240)
        self.assertLessEqual(len(str(new)),220)
        self.assertIn('Olav Lægreid',str(new))
    def test_configure_compacts_only_generated_paths_and_preserves_unicode_source(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)/'Olav Lægreid';root.mkdir();tools=self.tools(root)
            with mock.patch.object(b,'ROOT',root),mock.patch.object(b,'run',return_value='') as run:
                build=b.configure_checked_project('cmake',root/'build/j/u-123',tools)
            command=run.call_args.args[0]
            self.assertEqual(command[command.index('-S')+1],root)
            self.assertEqual(build.parent,root/'build')
            self.assertEqual(len(build.name),14)
            self.assertIn('-DCMAKE_OBJECT_PATH_MAX=220',command)
    def test_impossibly_deep_developer_checkout_stops_before_compiler(self):
        root=Path('/deep')/('nested'*30)
        with mock.patch.object(b,'ROOT',root),mock.patch.object(b,'run') as run,self.assertRaisesRegex(RuntimeError,'for dyp'):
            b.configure_checked_project('cmake',root/'JUCE',self.tools(root))
        run.assert_not_called()
    def test_actual_cmake_generation_hashes_the_long_object_path(self):
        cmake=shutil.which('cmake');ninja=shutil.which('ninja')
        if not ninja and os.name=='nt':
            base=Path(os.environ.get('ProgramFiles(x86)','C:/Program Files (x86)'))/'Microsoft Visual Studio'
            ninja=next((str(p) for p in base.glob('*/*/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe')),None)
        if not cmake or not ninja:self.skipTest('CMake/Ninja absent; the actual Windows build remains mandatory')
        temp_parent=os.environ.get('RUNNER_TEMP') or tempfile.gettempdir()
        with tempfile.TemporaryDirectory(prefix='fp-',dir=temp_parent) as temp:
            root=Path(temp)/'Olav Lægreid';root.mkdir()
            source=root/'build/j/u-0123456789abcdef0123/source_directory_for_real_cmake_path_hashing_safety_probe_0123456789/modules/juce_audio_processors_headless/juce_audio_processors_headless_lv2_libs.cpp'
            source.parent.mkdir(parents=True);source.write_text('int object_path_probe(){return 1;}\n',encoding='utf-8')
            # A forced compiler descriptor avoids invoking any compiler. This
            # probe measures CMake's actual generated filenames, not codegen.
            (root/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.22)\nproject(ObjectPathProbe LANGUAGES CXX)\nadd_library(FlysnapperProcessorAudioTests STATIC "'+source.relative_to(root).as_posix()+'")\n',encoding='utf-8')
            build=root/'b'
            command=[cmake,'-S',str(root),'-B',str(build),'-G','Ninja','-DCMAKE_MAKE_PROGRAM='+ninja,
                     '-DCMAKE_OBJECT_PATH_MAX=220','-DCMAKE_CXX_COMPILER='+sys.executable,
                     '-DCMAKE_CXX_COMPILER_ID=GNU','-DCMAKE_CXX_COMPILER_ID_RUN=TRUE','-DCMAKE_CXX_COMPILER_VERSION=14.2',
                     '-DCMAKE_CXX_COMPILER_WORKS=TRUE','-DCMAKE_CXX_COMPILER_FORCED=TRUE','-DCMAKE_CXX_ABI_COMPILED=TRUE']
            result=subprocess.run(command,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,encoding='utf-8',errors='replace')
            self.assertEqual(result.returncode,0,result.stdout)
            text=(build/'build.ninja').read_text(encoding='utf-8')
            objects=re.findall(r'^build (.+?\.(?:o|obj)): CXX_COMPILER',text,re.MULTILINE)
            self.assertEqual(len(objects),1,text)
            paths=[build/obj.replace('$ ',' ').replace('$:',':') for obj in objects]
            self.assertTrue(all(len(str(p))<=220 for p in paths),[(str(p),len(str(p))) for p in paths])
            self.assertTrue(any(re.search(r'(?:^|[/\\])[0-9a-f]{32}(?:[/\\]|$)',str(p)) for p in paths),objects)
            # Preserve actual generation proof with the test's native host path.
            self.assertIn('Olav Lægreid',str(paths[0]))

if __name__=='__main__':unittest.main()

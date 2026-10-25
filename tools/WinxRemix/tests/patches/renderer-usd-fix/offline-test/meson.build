project('winxremix_usd_exporter_check', 'cpp', version : '1',
  default_options : ['cpp_std=vc++17', 'buildtype=release', 'b_vscrt=md'])

source = get_option('renderer_source')
build = get_option('renderer_build')
usd = get_option('usd_directory')
if source == '' or build == '' or usd == ''
  error('Explicit renderer_source, renderer_build and usd_directory are required')
endif
if host_machine.system() != 'windows' or host_machine.cpu_family() != 'x86_64'
  error('This fixture requires Windows x64 and the pinned MSVC/USD build')
endif

add_project_arguments('/DNOMINMAX', '/DNDEBUG', '/Zc:__cplusplus', '/MP1', '/bigobj', language : 'cpp')
libraries = [build / 'src/util/libutil.a', build / 'src/tracy/libtracy.a']
# A previously compiled exporter object can isolate a source change without
# rebuilding renderer shaders. It must match the same headers and USD ABI.
exporter = get_option('exporter_object')
if exporter == ''
  libraries += [build / 'src/lssusd/liblssUsd.a']
else
  libraries += [exporter]
endif
foreach library : ['ar', 'arch', 'js', 'gf', 'kind', 'sdf', 'sdr', 'ndr', 'pcp', 'tf',
                   'trace', 'usd', 'usdLux', 'usdGeom', 'usdRender', 'usdShade', 'usdSkel',
                   'work', 'vt', 'python310', 'plug', 'tbbmalloc', 'boost_python310-vc142-mt-x64-1_76', 'tbb']
  libraries += [usd / 'lib' / (library + '.lib')]
endforeach
libraries += ['kernel32.lib', 'user32.lib', 'shell32.lib', 'shlwapi.lib',
              'version.lib', 'ole32.lib', 'advapi32.lib', 'psapi.lib']

executable('test_usd_exporter', files('../test_usd_exporter.cpp'),
  include_directories : include_directories(source / 'src/lssusd', source / 'include',
    source / 'include/vulkan/include', usd / 'include', usd / 'include/boost'),
  link_args : libraries, install : false)

executable('test_usd_dynamic_buffers', files('../test_usd_dynamic_buffers.cpp'),
  include_directories : include_directories(source / 'src/lssusd', source / 'include',
    source / 'include/vulkan/include', usd / 'include', usd / 'include/boost'),
  link_args : libraries, install : false)

executable('test_usd_variable_skin', files('../test_usd_variable_skin.cpp'),
  include_directories : include_directories(source / 'src/lssusd', source / 'include',
    source / 'include/vulkan/include', usd / 'include', usd / 'include/boost'),
  link_args : libraries, install : false)

executable('test_usd_signed_adapter', files('../test_usd_signed_adapter.cpp'),
  include_directories : include_directories(source / 'src/lssusd', source / 'include',
    source / 'include/vulkan/include', usd / 'include', usd / 'include/boost'),
  link_args : libraries, install : false)

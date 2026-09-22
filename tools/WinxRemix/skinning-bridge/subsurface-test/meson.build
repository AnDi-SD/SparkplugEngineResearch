project('winxremix_subsurface_wire', 'cpp', version : '1',
  default_options : ['cpp_std=vc++17', 'buildtype=release', 'b_vscrt=mt'])

# Own CPU fixture; both inputs must be separate, immutable source snapshots.
add_project_arguments('/DNOMINMAX', '/DNDEBUG', '/MP1', language : 'cpp')
foreach kind : ['baseline', 'candidate']
  source = get_option(kind + '_source')
  if source == ''
    error('An explicit ' + kind + '_source snapshot is required')
  endif
  executable('material_' + kind,
    files('../test_subsurface_wire.cpp', source / 'bridge/src/util/util_remixapi.cpp'),
    include_directories : include_directories(source / 'bridge/src/util', source / 'public/include'),
    install : false)
endforeach

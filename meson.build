project(
  'local-competitive-grader',
  'cpp',
  version: '0.1.0',
  meson_version: '>=1.10.0',
  license: 'MIT',
  default_options: [
    'cpp_std=c++23',
    'warning_level=3',
    'buildtype=debugoptimized',
    'b_lto=true',
    'b_colorout=always',
  ],
)

cmake = import('cmake')

opts = cmake.subproject_options()
opts.add_cmake_defines(
  {
    'REPROC++': true,
  },
)

reproc_proj = cmake.subproject('reproc', options: opts)
reproc_dep = reproc_proj.dependency('reproc++')

json = dependency('nlohmann_json', required: true)

subdir('src')
subdir('tests')

sanitize_opts = []
if get_option('buildtype') == 'debugoptimized'
  if host_machine.system() == 'linux'
    sanitize_opts = ['b_sanitize=address,undefined,leak']
  elif host_machine.system() == 'darwin'
    sanitize_opts = ['b_sanitize=address,undefined']
  endif
  catch2 = dependency('catch2-with-main', required: true)
endif

src_inc = include_directories('src')

grader_lib = static_library(
  'grader',
  sources: src_files,
  include_directories: src_inc,
  dependencies: [reproc_dep, json],
  override_options: sanitize_opts,
)

executable(
  'grade',
  'src/main.cpp',
  include_directories: src_inc,
  dependencies: [reproc_dep, json],
  override_options: sanitize_opts,
  link_with: [grader_lib],
  build_subdir: 'bin',
)

if get_option('buildtype') == 'debugoptimized'
  grader_test = executable(
    'grader_test',
    sources: test_files,
    include_directories: src_inc,
    dependencies: [reproc_dep, json, catch2],
    override_options: sanitize_opts,
    link_with: [grader_lib],
    build_subdir: 'bin',
  )
  test('grader_test', grader_test)
endif

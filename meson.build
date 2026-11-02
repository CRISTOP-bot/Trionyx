project(
  'Trionyx basics',
  ['cpp', 'c'],
  version: '1',
  default_options: [
    'cpp_std=c++23',
    'c_std=c23',
    'warning_level=3',
    'buildtype=debug',
  ],
)

inc = include_directories('include')
sources = files(
  'src/init/init.cpp',
  'src/init/init_parser.cpp',
  'src/props/props.c',
  'src/util/stdfunc.cpp',
)
sources2 = files(
  'src/power/powerctl.cpp',
)
sources3 = files(
  'src/props/props.c',
  'src/util/propctl.cpp',
)
sources += run_command('sh', '-c', 'find src/modules -type f -name "*.cpp" | sort', check: true).stdout().strip().split('\n')
executable(
  'powerctl',
  sources2,
  include_directories: inc,
  link_args: ['-static', '-pthread'],
  install: false,
)
executable(
  'propctl',
  sources3,
  include_directories: inc,
  link_args: ['-static', '-pthread'],
  install: false,
)
executable(
  'system-init',
  sources,
  include_directories: inc,
  cpp_args: ['-fno-exceptions', '-pthread'],
  c_args: ['-pthread'],
  link_args: ['-static', '-pthread'],
  install: false,
)

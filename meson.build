project('chiconyir', 'c', version: '0.1')

plugins_install_dir = '@0@/gstreamer-1.0'.format(get_option('libdir'))

cdata = configuration_data()
cdata.set_quoted('PACKAGE', meson.project_name())
cdata.set_quoted('VERSION', meson.project_version())
configure_file(output : 'config.h', configuration : cdata)

gst_deps = dependency('gstreamer-1.0 gstreamer-video-1.0')

shared_module('chiconyir',
  'src/gstchiconyirdec.c',
  c_args : ['-DHAVE_CONFIG_H=1'],
  link_args : ['-Wl,-no-undefined'],
  dependencies : gst_deps,
  install : true,
  install_dir : plugins_install_dir)

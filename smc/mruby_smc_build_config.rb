# Build configuration file for mruby.

# Absolute path to the directory this file resides in,
# independent of any PWD or invocation stuff.
THIS_DIR = File.expand_path(File.dirname(__FILE__))

config = lambda do |conf|
  # Some standard things included with mruby
  conf.gem '#{root}/mrbgems/mruby-math' # Math
  conf.gem '#{root}/mrbgems/mruby-time' # Time
  conf.gem '#{root}/mrbgems/mruby-struct' # Struct
  conf.gem '#{root}/mrbgems/mruby-sprintf' # #sprintf
  conf.gem '#{root}/mrbgems/mruby-string-ext' # More string stuff
  conf.gem "#{root}/mrbgems/mruby-array-ext" # Arrays

  # Additional things
  conf.gem "#{THIS_DIR}/../mruby/mgems/mruby-sleep"         # Sleep
  conf.gem "#{THIS_DIR}/../mruby/mgems/mruby-pcre-regexp"   # PCRE Regular Expressions
  #conf.gem "#{THIS_DIR}/../mruby/mgems/mruby-simple-random" # #rand, #srand # Does not work on Windows
  conf.gem "#{THIS_DIR}/../mruby/mgems/mruby-md5"           # MD5
end

MRuby::Build.new do |conf|
  toolchain :gcc
  config.call(conf)
end

if ENV["CROSSCOMPILE_TARGET"] and !ENV["CROSSCOMPILE_TARGET"].empty?
  prefix = ENV["CROSSCOMPILE_TARGET"]

  MRuby::CrossBuild.new(prefix) do |conf|
    toolchain :gcc

    conf.cc do |cc|
      cc.command = ENV["CC"] || "#{prefix}-gcc"
    end

    conf.linker do |linker|
      linker.command = ENV["LD"] || "#{prefix}-gcc"
    end

    conf.archiver do |archiver|
      archiver.command = ENV["AR"] || "#{prefix}-ar"
    end

    config.call(conf)
  end
end

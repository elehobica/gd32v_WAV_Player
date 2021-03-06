#!ruby
#-------------------------------------------------------
# Copyright (c) 2020, Elehobica
# Released under the BSD-2-Clause
# refer to https://opensource.org/licenses/BSD-2-Clause
#-------------------------------------------------------

require 'time'
Vinyl_thres_time = Time.parse("2018/03/08 00:00:00")

# config
# - key: コピー先のフォルダ名
# - cfg_DST_DIR_MAX_SIZE = config[0]
#  コピー先のサイズ
# - cfg_SRC_DIRS = config[1]
#  ソースのフォルダ名
#   以下例
#   g:/Data/Media/Audio/*/*
#   g:/Data/Media/Audio_exclude/Download_Rental/*/*
#   g:/Data/Media/Audio_exclude/Cassette_MD/*/*
#      g:/Data/Media/Audio_exclude/Download_Rental/*/*
# - cfg_FLAT_MODE = config[2]
#  0: アーティストフォルダ名\アルバム名\*.mp3
#  1: アーティストフォルダ名-アルバム名\*.mp3

CONFIGS = {
  _SiPeed_WAV_64GB: [
    61.5*1000*1000*1000, %w(
      g:/Data/Media/Audio/*/*
    ), 0
  ],
  _SiPeed_WAV_512GB: [
    509.5*1000*1000*1000, %w(
      g:/Data/Media/Audio/*/*
    ), 0
  ]
}

def get_size(path)
  expath = File.expand_path(path)
  sum = 0
  if (FileTest.directory?(expath))
    Dir.glob("#{expath}/**/*") do |fn|
      # next if File.directory?(fn)
      sum += File.stat(fn).size
    end
  else
    sum = File.stat(expath).size
  end
  sum
end

def symlink(from, to, switch='')
  from.gsub!('/', "\\")
  to.gsub!('/', "\\")
  open("|cmd.exe", "w") do |cmd|
    cmd.print "mklink #{switch} \"#{to.encode("Shift_JIS")}\" \"#{from.encode("Shift_JIS")}\"\n"
  end
end

def symlink_dir(from, to)
  symlink(from, to, '/D')
end

def make_directory(dir)
  return if (FileTest.directory?(dir))
  make_directory(File.dirname(dir))
  Dir.mkdir(dir)
end

def dir_size(path)
  expath = File.expand_path(path)
  sum = 0
  Dir.glob("#{expath}/**/*") do |fn|
    # next if File.directory?(fn)
    sum += File.stat(fn).size
  end
  sum
end

def rm_empty_dir(path)
  Dir.entries(path).each do |item|
    next if (/^\.\.?$/ =~ item)
    if (FileTest.directory?("#{path}/#{item}"))
      if (Dir.entries("#{path}/#{item}").size <= 2)
        Dir.rmdir("#{path}/#{item}")
      else
        rm_empty_dir("#{path}/#{item}")
      end
    end
  end
  # after care in case that parent directory gets empty after sub directory is removed
  if (Dir.entries(path).size <= 2)
    Dir.rmdir(path)
  end
end

### MAIN ###
#key = '_SiPeed_WAV_64GB'
key = '_SiPeed_WAV_512GB'
config = CONFIGS[key.to_sym]
cfg_DST_DIR = "g:/Data/Media/#{key}"
cfg_DST_DIR_MAX_SIZE = config[0]
cfg_SRC_DIRS = config[1]
cfg_FLAT_MODE = config[2]
dst_dirs = Array.new

#system("rm -rf \"#{cfg_DST_DIR}\"/*")

dirs_flat = Array.new
cfg_SRC_DIRS.each do |dir|
  dirs_flat.concat(Dir[dir])
end

total_size = 0

system("cp -f logo.bin \"#{cfg_DST_DIR}\"")

dirs_flat.sort{ |a, b| 
  # フォルダ以下のmp3ファイルの新しい順にソート
  aa = a.gsub(/([\[\]])/){ "\\" + $1 } #[]のエスケープ
  bb = b.gsub(/([\[\]])/){ "\\" + $1 } #[]のエスケープ
  aa_mp3_files = Dir["#{aa}/*.mp3"]
  bb_mp3_files = Dir["#{bb}/*.mp3"]
  if (aa_mp3_files.size == 0)
    1
  elsif (bb_mp3_files.size == 0)
    -1
  elsif (/\/\d+v\-/ =~ aa && /\/\d+v\-/ !~ bb && File.mtime(aa_mp3_files[0]) < Vinyl_thres_time)
    1
  elsif (/\/\d+v\-/ =~ bb && /\/\d+v\-/ !~ aa && File.mtime(bb_mp3_files[0]) < Vinyl_thres_time)
    -1
  else
    File.mtime(bb_mp3_files[0]) <=> File.mtime(aa_mp3_files[0])
  end
}.each do |dir|
  new_parent_dir = cfg_DST_DIR + '/' + File.basename(File.dirname(dir))
  if (cfg_FLAT_MODE == 1)
    src_dir = dir
    dst_dir = new_parent_dir + '-' + File.basename(dir) 
  else
    src_dir = dir
    dst_dir = new_parent_dir + '/' + File.basename(dir) 
  end
  printf "%s %s\n", src_dir, dst_dir
  STDOUT.flush
  #system("cp -r \"#{src_dir}\" \"#{dst_dir}\"")
  if (!FileTest.directory?(dst_dir))
    make_directory(dst_dir)
  end
  Dir["#{src_dir}/*.mp3"].each do |mp3|
    wav = File.basename(mp3, ".mp3") + ".wav"
    if (!FileTest.exist?("#{dst_dir}/#{wav}"))
        system("ffmpeg.exe -i \"#{mp3}\" -vn -ac 2 -acodec pcm_s16le -n -f wav \"#{dst_dir}/#{wav}\"")
    end
  end
  if (!FileTest.exist?("#{dst_dir}/cover.bin"))
    jpgs = Dir["#{src_dir}/*.jpg"]
    if (jpgs.size > 0)
        system("python jpg2bin.py \"#{jpgs[0]}\" \"#{dst_dir}/cover.bin\"")
    else
        mp3 = Dir["#{src_dir}/*.mp3"][0]
        system("ffmpeg.exe -y -i \"#{mp3}\" ./cover.jpg\"")
        system("python jpg2bin.py ./cover.jpg \"#{dst_dir}/cover.bin\"")
    end
  end
  total_size += get_size(dst_dir)
  if (total_size > cfg_DST_DIR_MAX_SIZE)
    system("rm -rf \"#{dst_dir}\"")
    break
  end
  dst_dirs.push(dst_dir)
end


if (cfg_FLAT_MODE == 1)
  storage_dirs = Dir[cfg_DST_DIR + "/*"]
else
  storage_dirs = Dir[cfg_DST_DIR + "/*/*"]
end

(storage_dirs - dst_dirs).each do |item|
  system("rm -rf \"#{item}\"")
end

rm_empty_dir(cfg_DST_DIR)


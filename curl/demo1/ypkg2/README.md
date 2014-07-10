Usage: ypkg command pkg1 [pkg2 ...]

ypkg is a simple command line for dealing with ypk package.

Commands:
  -h|--help                   Show usage
  -C|--remove                 Remove package
  -I|--install                Install package (pkg is leafpa.ypk not leafpad)
  -c|--check                  Check dependencies of package (pkg is leafpad.ypk not leafpad)
  -l|--list-files             List all files of installed package
  -k|--depend                 Show dependency of package
  -x|--unpack-binary          Unpack ypk package 
  -X|--unpack-info            Unpack ypkinfo 
  -b|--pack-directory         Pack directory to package
  -L|--list-installed         List all installed packages
  -s|--whatrequires           Show which package needs package
  -S|--whatprovides           Search which package provide this file
  --compare-version           Comprare two version strings 
  --color                     Colorize the output

Options:
  -f|--force                  Override problems, Only work with install
       

Get-ChildItem $args[0] -Recurse -Include *.cpp,*.hpp,*.h,*.cc,*.cxx | ForEach-Object { clang-format -i $_.FullName }

all: clean compile run

compile: utils.h utils.c makeFileSystem.c fileSystemOper.h fileSystemOper.c
	@echo "-------------------------------------------"
	@echo "Compiling all files..."
	@gcc -o makeFileSystem utils.c makeFileSystem.c
	@gcc -o fileSystemOper utils.c fileSystemOper.c -lm
run:
	@echo "-------------------------------------------"
	@echo "Running the program..."
	@echo "================================================================================="
	# UNCOMMENT THE COMMANDS THAT YOU WANT TO USE
	#./makeFileSystem 1 mySystem.data
	#./makeFileSystem 0.5 mySystem.data

	#./fileSystemOper mySystem.data mkdir "/usr"
	#./fileSystemOper mySystem.data mkdir "/usr/ysa"
	#./fileSystemOper mySystem.data mkdir "/bin/ysa"
	#./fileSystemOper mySystem.data write "/usr/ysa/file1" linuxfile.data 
	#./fileSystemOper mySystem.data write "/usr/file2" linuxfile.data
	#./fileSystemOper mySystem.data write "/file3" linuxfile.data
	#./fileSystemOper mySystem.data dir "/"
	#./fileSystemOper mySystem.data read "/usr/file2" linuxfile2.data
	#cmp linuxfile2.data linuxfile.data 
	#./fileSystemOper mySystem.data chmod "/usr/file2" -rw
	#./fileSystemOper mySystem.data read "/usr/file2" linuxfile2.data
	#./fileSystemOper mySystem.data chmod "/usr/file2" +rw
	#./fileSystemOper mySystem.data addpw "/ysa/file2" test1234
	#./fileSystemOper mySystem.data addpw "/usr/file2" test1234
	#./fileSystemOper mySystem.data read "/usr/file2" linuxfile2.data
	#./fileSystemOper mySystem.data read "/usr/file2" linuxfile2.data test1234
	@echo "================================================================================="
	@echo "Program exited...."

clean:
	@echo "-------------------------------------------"
	@echo "Removing files..."
	@rm -f *.o
	@rm -f makeFileSystem
	@rm -f fileSystemOper

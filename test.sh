# Clean

make clean
make
mkdir -p dist/


# Test Samples

valgrind ./mrcfg -c samples/test.mrc
valgrind ./mrcfg -c samples/test0.mrc
valgrind ./mrcfg -c samples/test1.mrc
valgrind ./mrcfg -c samples/test2.mrc
ABC="Wow!" valgrind ./mrcfg -c samples/test3.mrc
valgrind ./mrcfg -c samples/test4.mrc
ABC="Wow!" valgrind ./mrcfg -c samples/test5.mrc
valgrind ./mrcfg -c samples/test6.mrc
valgrind ./mrcfg -c samples/test7.mrc
valgrind ./mrcfg -c samples/test8.mrc
ABC="Wow!" valgrind ./mrcfg -c samples/test9.mrc


# Compile Test Samples

valgrind ./mrcfg -c samples/test.mrc -o dist/test.bin
valgrind ./mrcfg -c samples/test0.mrc -o dist/test0.bin
valgrind ./mrcfg -c samples/test1.mrc -o dist/test1.bin
valgrind ./mrcfg -c samples/test2.mrc -o dist/test2.bin
ABC="Wow!" valgrind ./mrcfg -c samples/test3.mrc -o dist/test3.bin
valgrind ./mrcfg -c samples/test4.mrc -o dist/test4.bin
ABC="Wow!" valgrind ./mrcfg -c samples/test5.mrc -o dist/test5.bin
valgrind ./mrcfg -c samples/test6.mrc -o dist/test6.bin
valgrind ./mrcfg -c samples/test7.mrc -o dist/test7.bin
valgrind ./mrcfg -c samples/test8.mrc -o dist/test8.bin
ABC="Wow!" valgrind ./mrcfg -c samples/test9.mrc -o dist/test9.bin


# Load Test Samples

valgrind ./mrcfg -l dist/test.bin
valgrind ./mrcfg -l dist/test0.bin
valgrind ./mrcfg -l dist/test1.bin
valgrind ./mrcfg -l dist/test2.bin
valgrind ./mrcfg -l dist/test3.bin
valgrind ./mrcfg -l dist/test4.bin
valgrind ./mrcfg -l dist/test5.bin
valgrind ./mrcfg -l dist/test6.bin
valgrind ./mrcfg -l dist/test7.bin
valgrind ./mrcfg -l dist/test8.bin
valgrind ./mrcfg -l dist/test9.bin

# libmali
modprobe -v mali_kbase
while ! [ -e /dev/mali0 ]
do
	sleep 0.2
done

sleep 1

chmod -R 777 /dev/mali0 /dev/dma_heap
#ldconfig

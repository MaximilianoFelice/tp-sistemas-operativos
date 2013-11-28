num=0
while [ $num -lt 600 ]
do
	truncate -s 0 $num
	num=`expr $num + 1`
done
while [ $num -lt 1200 ]
do
	mkdir $num
	num=`expr $num + 1`
done


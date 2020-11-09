#!/bin/bash
FILE_DIR=./pla/*.pla
USAGE="usage: $0 [g-m] [ERROR] [OUTPUT-FILE]"

if [ $# -ne 3 ] 
then
    echo "numero errato di argomenti"
    echo "$USAGE"
    exit -1
fi

if [ $1 != "g" ] && [ $1 != "m" ]
then
    echo "$USAGE"
    exit -1
fi

echo "NomeFile; Ct; r [%]; oldAND ;oldOR ;newAND ;newOR ;percAND [%] ;percOR [%]; CPUtime [s]" > "$3"

if [ $? -ne 0 ]
then
   echo "impossibile scrivere sul file $3".
   exit -1
fi

for file in $FILE_DIR; do
    ./main "-t" "-$1" "$2" "$file" >> "$3"
    if [ $? -ne 0 ]; then
        echo "errore con il test $file".
    else
		out=$(basename "$file")
		mv "./out/best.pla" "./temp/$out"
	fi
    sleep 0.1
done

unix2dos "$3"

exit 0
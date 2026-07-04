for i in {1..7}; do python chain.py list_${i}.txt --gap 0 --output montage_${i}.png; done
montage montage_*.png -tile 1x -geometry +0+20 montage_all.png

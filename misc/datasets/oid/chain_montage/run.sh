rm /tmp/oid_montage_*.png
for i in list_*.txt; do python chain.py ${i} --gap 0 --output /tmp/oid_montage_${i}.png; done
montage /tmp/oid_montage_*.png -tile 1x -geometry +0+20 montage_all.png
rm /tmp/oid_montage_*.png

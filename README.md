# MA4830-MajorCA

## Copy from Lab PC to USB
1. Plug your USB drive
2. Check your USB directory, by running `ls /fs/`, usually it is hd10-dos1
3. Copy your files to the USB by running `cp /your/files/directory/file-name /your/destination/folder/file-name`. 
4. For example: if the directory of your USB is hd10-dos1, and the file you want to copy is at /home/guest/ds/sine.c. You can run `cp /home/guest/ds/sine.c /fs/hd10-dos1/sine.c`
5. Ensure that the files are successfully copied by running `ls /fs/hd10-dos1`
6. Unmount the USB drive by runnning: `umount /fs/your_USB_drive_name_directory`, in this case it will be `umount /fs/hd10-dos1`
7. Done ~

Note: If you want to copy a directory use -R in the cp command: `cp -R /your/files/directory /your/destination/folder`

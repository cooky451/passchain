### [Dynamic search function allows you to find every entry in seconds.](anker0)

* **Why should I use a password manager?**
  * It allows your password security to be independent of the security flaws of the services you use. Every time you register for a service with the same password, you give up your security to them. Will they store the password properly? What happens if they get hacked, will the hackers be able to use your stolen password for other, more important services?
  * It allows you to use strong, unique passwords for everything. And you don't need to remember them! That website that forced you to use two instead of one special character and now you can't remember anymore how you modified your default password for them? A thing of the past!
  * You also don't have to remember usernames anymore. Did you have to log in with your nick name or with your E-Mail address for this service? It's all stored in your password manager.

* **What happens if I need an older password?**
  * Passchain stores a complete history of all usernames and passwords, so looking up old ones is no problem.

* **What happens if the device goes to sleep with the password manager open? Will my passwords get written to disk?**
  * Passchain listens for low powerstate events and automatically erases the temporary encryption key from memory and then goes into standby mode. Your passwords will not be written to disk, but you'll have to enter your master password again after resuming.
  * Passchain also automatically does this if the device is idle for more than 20 minutes, even if it doesn't go into a sleep or hibernation mode.

* **Can I configure the automatic timeout time?**
  * Yes, the configuration file is in "%APPDATA%/passchain" folder. It's not stored inside the database or besides the executable because these settings represent properties of the device and not the program. For example, you might want to have a lower idle timeout time for a laptop or work PC than a desktop at home.
  * A value of 0 means that passchain will never automatically go into standby mode because the device being idle. Passchain will always go to standby mode if the device is going to sleep or hibernate.

* **Do I have to be careful about where to store my database file?**
  * Generally no, the whole file is encrypted and as long as your password is sufficiently strong an adversary cannot gain any information from it. (Except it's size and the version of passchain you're using.)
  * A complete description of the file format can be found in src/database.hpp

* **Which kind of encryption does passchain use?**
  * The database file is encrypted using ChaCha20. The MAC of the database file is calculated using SHA-3-256. The encryption key is derived from your master password, a 256-bit random salt/nonce and 10002 iterations of SHA-3-256.

* **Since passchain encrypts passwords in memory, does that mean it is save to use on a device I don't trust?**
  * *No, it is never a good idea to access any sensitive information on devices you don't trust.* While operating, passchain necessarily holds all the information needed to decrypt your passwords in memory. The usernames, passwords and comments are just encrypted to reduce the attack surface, a program simply scanning memory for strings will not be able to get anything from encrypted data. Should the memory ever be written to disk (for example becauses passchain is used in a virtual machine and the host saves it's state), it will be much harder to extract a temporary encryption key and try to identify what to decrypt than it would be to just scan the data for strings.
  * It also makes it easy to prevent sensitive data from being swapped out of memory onto disk, because passchain only has to prevent the 256 temporary key bits from being swapped out.
  * Regardless, if you are using or editing or accessing the data in any way it needs to be unencrypted in memory, so never, ever open your database on a device you do not trust.

* **Can I merge database files?**
  * You can currently export the whole database as text (again, only do this on device you trust, everything will be unencrypted and visible!) and import this text into an existing database. This also allows you to make very specific changes to it, like erasing entries from the history or manipulating timestamps.
  * It is recommended to not keep the export window open for too long, or leave the data in clipboard, or paste it into any editor that makes automatic backup copies like Word, to prevent it from being written to disk.
  * A feature to allow the merging of a binary file into an existing database will be implemented shortly.

* **Should I backup my database file?**
  * Absolutely! Having your database file on only one device is the worst thing you can do, if that device fails your passwords are gone. Luckily, the database file is encrypted and can be stored anywhere you want. Make regular copies to all devices you're using! Phones, USB-Sticks, external HDDs, google drive, whatever. Usually having it on two devices that aren't in the same location (so they won't get stolen or break in a fire at the same time) is enough to guarantee that you won't lose the file. But this one backup is absolutely needed and will save your ass! Of course, the more backups you make, the better.
  * You can also backup the executable file together with your database file if you want to, in case you're worried that you won't be able to get it anymore at some point in time.

<a name="anker0" />
![Screenshot](/screenshots/00.png?raw=true)
![Screenshot](/screenshots/01.png?raw=true)
![Screenshot](/screenshots/03.png?raw=true)
![Screenshot](/screenshots/02.png?raw=true)
![Screenshot](/screenshots/04.png?raw=true)

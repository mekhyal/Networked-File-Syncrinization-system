System Programming Project

The Project about Networked File Synchronization System Team leader: -> Abdulaziz Mohammed Menbers: -> Yousef Joudeh -> Abdullah Almekhyal

Description of Project: Here, we will have a server that keeps files for a user. The client requests to sync a local folder with the server. Initially, the server has an empty directory. The client syncs a directory to the server. This will create a replica of the local files on the server. Next, if the client makes changes to the local server, it can sync them (if the user wants) with the server. In case the files had changed on the server as well, those changes will be synced back to the client. Only NEW files are allowed from the server. No changes to existing files.

RULES/PROTOCOL:

The server must work concurrently with multiple clients, each having a different home directory to use for syncing.
The client must work with command line parameters to start syncing a specific path on the local machine with a specific path on the server machine.
If the user does not specify the destination path, it will automatically sync to the directory with the exact name of the local directory path.
The client must be able to change a part of the file. That is, say the file is split into two parts from byte 0 to len(file)/2, and from byte len(file)/2+1 to the END. If there was a change in the first part, only the first part is synced. If there was a change in the second part, only the second part is changed.
To determine if there was a change. For each file, split it into multiple (at least 2) chunks. Compute a checksum for each part. At the time of sync, recompute the checksums, if there was a change in any of the checksums, sync the part it represents with the server.
Weekly Tasks:

Week 1 (Apr 15 - Apr 21): Implement file syncing basics and local-to-server sync logic

Week 2 (Apr 22 - Apr 28): Implement checksum verification and part-wise sync

Week 3 (Apr 29 - May 5): Add concurrency for multi-client handling

Week 4 (May 6 - May 12): System testing, optimization, and integration.

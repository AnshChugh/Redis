
# Remote Dictionary Server (Redis)
  A remote dictionary server for storing key-value pairs on a remote server


### Sample Protocol
	4 byte length followed by bytes (length) then another 4 byte length followed by corresponding
		length of bytes


### UPDATE: 
poll or WSAPoll() is broken on windows. I've reached a dead end on WSAPoll Error: 10022
There is barely any resources for event loop in winsock using poll.
might continue in the future if i find a breakthrough.

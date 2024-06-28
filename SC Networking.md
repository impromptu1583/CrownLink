## A partial reverse engineering of some of the starcraft networking messages

### Message structure - header+body
The messges I observed seem to have a 12-byte header followed optionally by data. Here is the header structure as far as I can tell:

```
Timer | Message Type | Conf Seq | Send Seq |   ?   | From 
xx xx |    0d  00    |  08  00  |  1a  00  | 02 00 | 01 00 
```

- Timer: appears to be a short int that increments until it overflows and restarts. Because it's only two bytes it overflow and restarts every few seconds but it's enough to detect if you got a packet out of order. I may be misunderstanding this field but that's my best guess
- Message Type: I've listed the various types I've seen below
- Send Seq / Conf Seq: Each message has a confirmation of the last message received from the peer followed by a sequence number for this message. Some message types seem to have their own sequence while others are more global.
- ? unknown u16. I at first thought it was a u32 with the next field but it seems to be used for other purposes in some messages
- From: typically messages from the host will be `00 00` and messages from the first non-host player will be `01 00`. Needs more investigation to confirm whether next player would be `02 00` or all messages to a host are `01 00`.

## Some message types I've observed, roughly in the order they happen in a game:

### Type 0d 00 Keepalive

Seems to be a keep-alive / ping type message. These are sent frequently when no other packets are being sent (like when waiting for a game countdown timer)
Example:
```
timer | message type | conf seq | send seq |   ?a  | from  | ?c
xx xx |    0d  00    |  08  00  |  1a  00  | 02 00 | 01 00 | 05
```

when connecting the single byte payload always seems to be `05`

when waiting on a user (e.g. drop player screen) the payload appears to be the user id)


### Type 10 00 Join Lobby

The first message sent to the host when joining their lobby. Also seems to be sent back as confirmation. Typically sent 3x.
```
timer | message type | conf seq | send seq | counter | from
xx xx |    10   00   |  01  00  |  1a  00  |  00 01  | ff 00
```
examples:
```
28 c4.10 00.00 00.01 00.00 01.ff 00 | 01 00 00 00 to host
33 b7 10 00 01 00 01 00.00 02.00 00 | 01 00 00 00 from host
40 a8 10 00 01 00 02 00.00 03.ff 00 | 01 00 00 00
```
Interestingly in this message type the from field works differently. `ff 00` is used player->host instead of `01 00`. I suspect this is because the player hasn't joined the lobby yet so technically does not have a player ID yet.

### Type 14 00 Player Name

This is sent quickly after type 10 above and only contains the player name as a c-string. The body appears to be padded out with `null` characters to an 8-byte alignment.

```
               header         to h  | J  E  S  S  E | c-string aligned to 8 bytes
6c 78.14 00.02 00.02 00.00 07.ff 00 | 4a 65 73 73 65 00 00 00
```
Similarly to type 10 above the 'from' value of `ff 00` is used.

### Type 4e 00 Map Data

This is sent from the host to the joining player. The payload is identical to advertisement messages and contains the map name and other game info.

```
               header        from h | players     | max players |    u32?     |    u32?     |    u32?
58 b0 4e 00 02 00 03 00 00 08 00 00 | 01 00 00 00 | 08 00 00 00 | 16 00 00 00 | 04 00 00 00 | 05 00 00 00  X.N.............................

  c-string name   |   map info
4a 65 73 73 65 00 | 2c 34 34 2c 2c 33   2c 2c 31 65 2c 2c 31 2c 63 62 32 65 64 61 61 62 2c 31 2c 2c          Jesse.,44,,3,,1e,,1,cb2edaab,1,,

  c-string name   | c-string map name + padding?
4a 65 73 73 65 0d | 41 78 69 6f 6d 0d 00 00                                                                  Jesse.Axiom...
```

### Type 37 00

Similar to type 14 but is sent from the server to the joining player. It seems likely that in a multiplayer lobby the host might send multiples of these messages to communicate all player names but I haven't had the chance to observer a multiplayer game yet.

```
               header        from h |         
41 3d 37 00 03 00 03 00 00 06 00 00 | 2b 00 00 00 | 00 00 00 00 | 01 00 00 00 | 00 00 00 00 | 16 00 00 00  A=7.........+...................
game name / player name 
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 4a 65 73 73 65 00 00                             ................Jesse..
```
Interestingly the player name portion of the message is left-padded to a total length of 23 characters, which is the exact maximum amount of characters that can be displayed in the games list. Not sure if this is coincidental or not.

### Type 2c 00

This message is sent from the host after type 37. Unsure of the conetents and purpose at this time.
```
               header        from h | 
a9 f1 2c 00 05 00 03 00 00 09 00 00 | 1e 00 01 00 00 00 00 00 00 00 01 02 00 01 03 01 00 00 00 00  ..,.............................
00 00 00 00 00 00 00 00 00 00 00 00 
```

### Type 0c 00

This seems to be another type of keepalive(similar to 0d) or perhaps a ping.
```
send 4d 99 0c 00 06 00 03 00 | 00 04 | 00 00
recv 69 7a 0c 00 03 00 07 00 | 00 05 | 01 00
recv 64 7f 0c 00 04 00 07 00 | 00 04 | 01 00
send 62 80 0c 00 07 00 05 00 | 00 05 | 00 00
(later)
recv 5f 82 0c 00 08 00 08 00 00 00 01 01 some kind of confirmation?
```
note the u16 column I've highlighted. This message happens after the type 10 messages which had a counter there 1-3. It's possible this value is continuing that counter, or maybe it means something else.

### Type 85

Not sure what this is yet. Perhaps some game info or initial random seed or something.
```
               header        from h |
a0 95 85 00 00 00 00 00 01 00 00 00 4b 20 00 15 fa ff ff ff 5c 82 39 00 0e 4c 0d 00 ad 45 72 12  ............K ......\.9..L...Er.
ad 45 72 12 0e 4c 0d 00 18 fd 19 20 00 55 85 77 00 00 00 00 c7 d3 88 64 f0 fc 19 00 00 00 00 00  .Er..L..... .U.w.......d........
6c 00 00 00 e8 ea b0 06 7c 20 00 77 f0 19 f5 75 00 00 f2 15 dc fc 19 00 19 00 00 00 e8 fc 19 00  l.......| .w...u................
00 00 00 00 d0 fc 19 82 00 71 03 15 d0 fc 19 00 6a 71 03 15 a8 eb 05 15 00 00 00 00 84 00 f2 15  .........q......jq..............
04 fd 19 00 09 
```

### Type 4b

Sent right after Type 85. Perhaps more game info.
```
               header       from h |
8a 1c 4b 00 01 00 00 00 01 00 00 00 4a 01 00 80 00 80 00 06 06 06 06 06 06 06 06 00 00 00 00 06  ..K.........J...................
06 02 02 02 02 02 02 00 00 00 00 06 06 06 06 06 06 06 06 00 00 00 00 01 01 04 04 04 04 04 04 01  ................................
0e 0e 0e 01 01 00 00 00 00 00 00 
```

### Type 1e

Another unknown message in the sequence.
```
               header         to h  |
6a 7b 1e 00 19 00 19 00 02 00 01 00 | 40 00 00 00 00 00 00 00 00 01 00 05 00 00 ab da 2e cb
```

### Type 11

Another unknown message in the sequence.
```
               header        from h |
53 4d 11 00 03 00 00 00 01 00 00 00 | 49 01 00 00 00
```

### Type 2a Map Filename

Sent from the host to the player(s) with the actual map filename.
```
               header        from h |
32 d5 2a 00 04 00 00 00 01 00 00 00 | 4f 1b 00 01 00 0e 48 01 00 63 be 1b 17 28 32 29 41 78 69 6f  2.*.........O.....H..c...(2)Axio
6d 20 31 2e 31 2e 73 63 78 00                                                                    m 1.1.scx.
```

### Type 17 to host

Another unknown message in the sequence. Sent right after 2a so perhaps a confirmation or hash of the map?
```
               header         to h  |
88 a9 17 00 00 00 05 00 01 00 01 00 | 4f 08 00 00 00 00 01 0e 48 01 00  ............O.......H..
```

### Type 4e Start Countdown

This message appears to be the countdown start. For future reference in the sequence this message was at 51.342               
```
               header        from h|
b3 2e 4e 00 1b 00 1c 00 02 00 00 00 3d 64 3e 07 ff 06 02 04 3e 06 ff 06 02 04 3e 05 ff 06 02 04  ..N.........=d>.....>.....>.....
3e 04 ff 06 02 04 3e 03 01 02 02 04 3e 02 00 02 02 04 3e 01 ff 06 06 01 3e 00 ff 06 06 01 3f 01  >.....>.....>.....>.....>.....?.
00 00 01 00 05 00 3f 00 00 00 01 00 05 00                                                        ......?.......
```
This message appears to lay out the countdown sequence. Each `3e` chracter (`>`) is followed by a descending number.

### Type 0e

This message is sent from the joined player to the host right after 4e above, so it's possibly a confirmation.
```
               header         to h |
64 ae 0e 00 1d 00 1d 00 02 00 01 00 3d 64
```

### Type 0f

These messages are sent during the countdown just before it ends. Perhaps one final confirmation?
```
               header        from h|
52 07 0f 00 28 00 29 00 02 00 00 00 44 00 00  R...(.).....D.. 54.592
86 c7 0f 00 2d 00 2e 00 02 00 00 00 44 01 00  ....-.......D.. 55.842
```

### Type 12 Final Countdown

These messages from the host happen at t-1.25 and t-0 seconds.
```
               header        from h|
d3 78 12 00 2a 00 2b 00 02 00 00 00 3e 00 ff 05 06 01  .x..*.+.....>..... 55.092 = 1.25 second warning?
08 39 12 00 2f 00 30 00 02 00 00 00 3e 01 ff 05 06 01  .9../.0.....>..... 56.342 = exactly 5 sec after countdown began
```

Right after the game starts another type 10 message is sent:
36 92 10 00 08 00 05 00 00 0e 00 00 0c 00 00 00  6...............
Then the host calls `spiStartAdvertising` to update the game state and the game begins,

### During the game

## Type 2b

These messages are actual game state data exchanged. Once the game begins primarily only these messages are exchanged.
```
               header        from h|
c7 90 2b 00 dc 00 dc 00 02 00 00 00 37 01 64 95 00 70 22 63 02 02 00 00 88 0e 00 00 00 00 00 63  ..+.........7.d..p"c...........c
02 02 00 00 88 0e 00 00 00 00 00                                                                 ...........
```

## Type 14

A blank type 14 message is sent when a player leaves (typically sent 3x). This one was of the host leaving:
```
               header        from h|
ed be 14 00 0b 00 07 00 00 0b 00 00 e0 00 00 00 01 00 00 40  ...................@
```

## Type 18

Sent to inform clients that an unresponsive client is being dropped after a user clicks "drop player"
```
               header        from h|
ed 16 18 00 0e 00 09 00 00 0c 00 00 02 00 00 00 77 01 00 00 06 00 00 40
```
This message is for dropping player 02 so possibly byte 13 is the player ID that is being dropped.
`00 0c` might have something to do with the message type as type `0c` messages are sent as pings to unresponsive users

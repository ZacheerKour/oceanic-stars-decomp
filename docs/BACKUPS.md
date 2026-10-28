# Save backups

Every time you press Play (or start with --play / Steam), the launcher copies your memory cards (`saves\*.mcd`) into `saves\backups\<date_time>\`.
It copies them again when the game closes, but only if a card changed.
The newest 30 backups are kept; older ones are deleted. Nothing outside `saves\backups` is ever deleted.
A card that is empty or damaged is never backed up, so a good backup is not replaced. A note goes to `play\launcher.log`.
Backups never touch your live saves, and a backup problem never stops the game from starting.

To restore: close the game, open the backup folder you want, and copy `card1.mcd` (and `card2.mcd` if present)
back into `saves\`, replacing the current file. Start the game again.
Tip: keep a copy of the current card first, in case you picked the wrong backup.

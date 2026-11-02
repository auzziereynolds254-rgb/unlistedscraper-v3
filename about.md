# Unlisted Scraper

Opens a profile, hits **SCAN**, and the mod pulls every level object the servers will return for that user — every page of the levels list, a name-search pass, and (optionally) profile comments — then probes the level ID space around each hit, keeping anything whose `userID` matches the target.

Unlisted levels don't appear in the levels list or in search, so the probe pass is what actually finds them: GD level IDs are allocated in upload order, so a creator's unlisted uploads sit in the same numeric neighbourhood as their public ones.

Results go to the clipboard and to `saveDir/scans/<userID>.json`, raw fields intact.
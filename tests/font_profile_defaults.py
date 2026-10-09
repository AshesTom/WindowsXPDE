#!/usr/bin/env python3
"""Clock migration defaults never overwrite existing user profile values."""
from pathlib import Path
import struct
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'prefix'))
import registry

class ClockDefaults(unittest.TestCase):
    def test_measured_clock_default(self):
        kind,data=registry.font_profile_defaults()['ClockFont']
        self.assertEqual(kind,'REG_BINARY')
        fields=struct.unpack('<5l8B64s',bytes.fromhex(data))
        self.assertEqual(fields[0],-12)
        self.assertEqual(fields[4],400)
        self.assertEqual(fields[5:13],(0,0,0,1,0,0,0,0))
        self.assertEqual(fields[13].decode('utf-16le').rstrip('\0'),'MS UI Gothic')
    def test_seed_only_missing(self):
        for existing in (False,True):
            commands=[]
            registry.install(lambda *args:commands.append(args),
                             lambda key,name:existing if key==registry.FONT_PROFILE and name=='ClockFont' else True)
            clocks=[args for args in commands if 'ClockFont' in args]
            self.assertEqual(len(clocks),0 if existing else 1)
            if clocks:self.assertEqual(clocks[0][clocks[0].index('/d')+1],registry.font_profile_defaults()['ClockFont'][1])
        self.assertGreaterEqual(registry.VERSION,6)

if __name__=='__main__':unittest.main()

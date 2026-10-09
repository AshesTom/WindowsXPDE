#!/usr/bin/env python3
"""Measured part-font migration defaults never overwrite existing user profile values."""
from pathlib import Path
import struct
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'prefix'))
import registry

class PartFontDefaults(unittest.TestCase):
    def test_measured_part_defaults(self):
        for name in ('ClockFont', 'TaskFont'):
            kind,data=registry.font_profile_defaults()[name]
            self.assertEqual(kind,'REG_BINARY')
            fields=struct.unpack('<5l8B64s',bytes.fromhex(data))
            self.assertEqual(fields[0],-12)
            self.assertEqual(fields[4],400)
            self.assertEqual(fields[5:13],(0,0,0,1,0,0,0,0))
            self.assertEqual(fields[13].decode('utf-16le').rstrip('\0'),'MS UI Gothic')
    def test_seed_only_missing(self):
        for name in ('ClockFont', 'TaskFont'):
            for existing in (False,True):
                commands=[]
                registry.install(lambda *args:commands.append(args),
                                 lambda key,value_name:existing if key==registry.FONT_PROFILE and value_name==name else True)
                clocks=[args for args in commands if name in args]
                self.assertEqual(len(clocks),0 if existing else 1)
                if clocks:self.assertEqual(clocks[0][clocks[0].index('/d')+1],registry.font_profile_defaults()[name][1])
        self.assertGreaterEqual(registry.VERSION,7)

if __name__=='__main__':unittest.main()

"""Independent decision-table expectations; no transport acceptance implied."""
import unittest
from policy import Outcome, Status, before_attempt, after_attempt, deadline_expired

O = Outcome
S = Status

class BoundaryTable(unittest.TestCase):
    def test_deadline_edges(self):
        self.assertFalse(deadline_expired(99,100))
        self.assertTrue(deadline_expired(100,100))
        self.assertTrue(deadline_expired(101,100))

    def test_preflight_table(self):
        cases = [
            ('empty dominates every gate', (0,0,False,True,True), O(0,S.COMPLETE)),
            ('finished progress dominates stop', (4,4,True,True,True), O(4,S.COMPLETE)),
            ('closed dominates stop and expiry', (4,0,False,True,True), O(0,S.ERROR,'bad_file_descriptor')),
            ('closed retains progress', (4,2,False,False,True), O(2,S.ERROR,'bad_file_descriptor')),
            ('stop dominates expiry', (4,0,True,True,True), O(0,S.CANCELLED)),
            ('partial stop dominates expiry', (4,2,True,True,True), O(2,S.CANCELLED)),
            ('expiry with zero bytes', (4,0,True,False,True), O(0,S.TIMEOUT)),
            ('expiry retains bytes', (4,2,True,False,True), O(2,S.TIMEOUT)),
            ('live permits attempt', (4,0,True,False,False), None),
            ('partial live permits attempt', (4,2,True,False,False), None),
        ]
        for label,args,want in cases:
            with self.subTest(label=label): self.assertEqual(before_attempt(*args), want)

    def test_attempt_table(self):
        cases = [
            ('some short data', (4,0,2,'data'), {'some':True}, O(2,S.PROGRESS)),
            ('some fills span', (4,0,4,'data'), {'some':True}, O(4,S.PROGRESS)),
            ('exact fills span', (4,0,4,'data'), {}, O(4,S.COMPLETE)),
            ('exact final fragment', (4,2,2,'data'), {}, O(4,S.COMPLETE)),
            ('all needs another fragment', (4,0,2,'data'), {'sending':True}, None),
            ('exact needs another fragment', (4,0,2,'data'), {}, None),
            ('EOF before progress', (4,0,0,'eof'), {}, O(0,S.EOF)),
            ('EOF after progress', (4,2,0,'eof'), {}, O(2,S.EOF)),
            ('error after progress', (4,2,0,'error'), {}, O(2,S.ERROR,'native_error')),
            ('count and error coexist', (4,1,2,'error'), {}, O(3,S.ERROR,'native_error')),
            ('count reaches size but error remains', (4,2,2,'error'), {}, O(4,S.ERROR,'native_error')),
            ('interrupted retry', (4,2,0,'retry'), {}, None),
            ('zero send is error', (4,2,0,'data'), {'sending':True}, O(2,S.ERROR,'no_progress')),
            ('unmapped zero receive is error', (4,2,0,'data'), {}, O(2,S.ERROR,'no_progress')),
        ]
        for label,args,kw,want in cases:
            with self.subTest(label=label): self.assertEqual(after_attempt(*args,**kw),want)

    def test_race_sequences(self):
        # Readiness itself never bypasses a fresh stop/expiry snapshot.
        self.assertEqual(before_attempt(4,0,stopped=True),O(0,S.CANCELLED))
        self.assertEqual(before_attempt(4,0,expired=True),O(0,S.TIMEOUT))
        # Intent arriving during a successful attempt cannot undo committed data.
        self.assertEqual(after_attempt(4,0,4,'data'),O(4,S.COMPLETE))
        self.assertEqual(after_attempt(4,0,2,'data',some=True),O(2,S.PROGRESS))
        self.assertIsNone(after_attempt(4,0,2,'data'))
        self.assertEqual(before_attempt(4,2,stopped=True,expired=True),O(2,S.CANCELLED))
        self.assertIsNone(after_attempt(4,2,0,'retry'))
        self.assertEqual(before_attempt(4,2,expired=True),O(2,S.TIMEOUT))
        self.assertEqual(after_attempt(4,2,0,'eof'),O(2,S.EOF))
        self.assertEqual(after_attempt(4,2,0,'error'),O(2,S.ERROR,'native_error'))

    def test_invalid_observations(self):
        for args in [(4,-1),(4,5),(-1,0)]:
            with self.subTest(args=args), self.assertRaises(ValueError): before_attempt(*args)
        for args in [(4,2,3,'data'),(4,2,-1,'data'),(4,2,1,'retry'),(4,2,1,'eof'),(4,2,0,'unknown')]:
            with self.subTest(args=args), self.assertRaises(ValueError): after_attempt(*args)
        with self.assertRaises(ValueError): after_attempt(4,2,0,'eof',sending=True)

if __name__ == '__main__': unittest.main(verbosity=2)

#!/usr/bin/perl

use strict;

my $size = ord('q') * ord('z');
for my $i (0..$size)
{
    if ($i == ord('"')) { print "&PagesExtractor::do_double_quote, ";}
    elsif ($i == ord("'")) { print "&PagesExtractor::do_quote, ";}
    elsif ($i == ord('B') * ord('q') + ord('T')) { print "&PagesExtractor::do_BT, ";}
    elsif ($i == ord('D') * ord('q') + ord('o')) { print "&PagesExtractor::do_Do, ";}
    elsif ($i == ord('E') * ord('q') + ord('T')) { print "&PagesExtractor::do_ET, ";}
    elsif ($i == ord('Q')) { print "&PagesExtractor::do_Q, ";}
    elsif ($i == ord('T') * ord('q') + ord('*')) { print "&PagesExtractor::do_T_star, ";}
    elsif ($i == ord('T') * ord('q') + ord('D')) { print "&PagesExtractor::do_TD, ";}
    elsif ($i == ord('T') * ord('q') + ord('J')) { print "&PagesExtractor::do_TJ, ";}
    elsif ($i == ord('T') * ord('q') + ord('L')) { print "&PagesExtractor::do_TL, ";}
    elsif ($i == ord('T') * ord('q') + ord('c')) { print "&PagesExtractor::do_Tc, ";}
    elsif ($i == ord('T') * ord('q') + ord('d')) { print "&PagesExtractor::do_Td, ";}
    elsif ($i == ord('T') * ord('q') + ord('f')) { print "&PagesExtractor::do_Tf, ";}
    elsif ($i == ord('T') * ord('q') + ord('j')) { print "&PagesExtractor::do_Tj, ";}
    elsif ($i == ord('T') * ord('q') + ord('m')) { print "&PagesExtractor::do_Tm, ";}
    elsif ($i == ord('T') * ord('q') + ord('s')) { print "&PagesExtractor::do_Ts, ";}
    elsif ($i == ord('T') * ord('q') + ord('z')) { print "&PagesExtractor::do_Tz, ";}
    elsif ($i == ord('c') * ord('q') + ord('m')) { print "&PagesExtractor::do_cm, ";}
    elsif ($i == ord('q')) { print "&PagesExtractor::do_q, ";}
    elsif ($i == ord('B') * ord('q') + ord('I')) { print "&PagesExtractor::do_BI, ";}
    else {print "nullptr, ";}
    print "\n" if ($i % 5 == 0);
}

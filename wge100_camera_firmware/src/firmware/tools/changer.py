import sys
import re

def alter(w):
    if re.match(r'-?[0-9]+\.?$', w):
        return 'd# ' + w
    else:
        return w

for src in sys.argv[1:]:
    new = []
    for l in open(src):
        words = l.split(" ")
        if 'h#' not in words:
            words = [alter(w) for w in words]
        new.append(" ".join(words))

    open(src, 'w').write("".join(new))

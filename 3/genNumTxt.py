with open('b10000.txt', 'w') as f:
    for i in range(0,10001,1):
        f.write('%d\n' % i)
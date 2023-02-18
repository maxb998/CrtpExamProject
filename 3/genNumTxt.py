with open('1000000.txt', 'w') as f:
    for i in range(0,1000001,1):
        f.write('%d\n' % i)
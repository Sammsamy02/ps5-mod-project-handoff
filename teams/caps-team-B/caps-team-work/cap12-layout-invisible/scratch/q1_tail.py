import sys
exec(open(sys.argv[1]+'/q1_test.py').read())
z,d,bulk=tex(sys.argv[3])
for i,b in enumerate(z.bulk_data): print(i,b)
for e in z.exports: print(e['name'], e['chunk_offset'], e['serial_size'])
print('header_size', z.header_size, 'chunk len', len(d))

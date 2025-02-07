import hashlib

def main():
    cut_lines = [
        [6, 6],
        [49, 51],
        [53, 53],
        [191, 191]
        ]
    with open('TinySTL/vector.h', 'rb') as file:
        raw = file.read()
    m = hashlib.sha3_256()
    m.update(raw)
    hexdigest = m.hexdigest()
    if hexdigest != 'bc64c381a83d719f521c10960232f6f67f1e70154c2cfa834da8538c95479251':
        print('vector.h has wrong version')
        exit(1)
    raw = raw.decode('utf-8')
    lines = raw.splitlines()
    for l, r in cut_lines:
        l -= 1
        for i in range(l, r):
            lines[i] = '// ' + lines[i]
    raw = '\n'.join(lines)
    with open('arch/wasm/include/vector.h', 'w') as file:
        file.write(raw)

if __name__ == "__main__":
    main()
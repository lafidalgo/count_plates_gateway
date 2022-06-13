try:
    import nanoid
except:
    import pip
    pip.main(["install","nanoid"])
    import nanoid

y = input("Quantos Digital Twins você quer?\n")

y = int(y)
for i in range(y):
    print(nanoid.generate(size=23))
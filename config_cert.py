import pip

try:
    from OpenSSL import crypto, SSL
    import random
    import nanoid
except:
    pip.main(["install", "pyopenssl"])
    pip.main(["install", "nanoid"])
    from OpenSSL import crypto, SSL
    import nanoid
    import random

root_cert_file = "./server_certs/rootCA.pem"
root_key_file = "./server_certs/rootCA.key"

client_cert_file = "./spiffs_certs/leaf_certificate.pem"
client_key_file = "./spiffs_certs/leaf_private_key.pem"

y = input("Voce ja tem o certificado base na pasta server_certs (de nome rootCA.pem)? digite s para sim e n para não\n")

person_name = input("Digite seu nome\n")

if (y != "s"):
    root_key = crypto.PKey()
    root_key.generate_key(crypto.TYPE_RSA, 2048*2)

    cert = crypto.X509()
    cert_name = "cert_"  + person_name + "_" + nanoid.generate(size=15) 
    cert.get_subject().CN = cert_name

    cert.set_serial_number(random.randint(10000000000000,10000000000000000000))
    cert.gmtime_adj_notBefore(0)
    cert.gmtime_adj_notAfter(20*365*24*60*60) #20 anos
    cert.set_issuer(cert.get_subject())
    ca_subj = cert.get_subject()
    cert.set_pubkey(root_key)
    cert.sign(root_key, 'sha256')

    print("O COMMON_NAME DO CERTIFICADO É: ", cert_name, "\n")

    with open(root_cert_file, "wb") as f:
        f.write(crypto.dump_certificate(crypto.FILETYPE_PEM, cert))

    with open(root_key_file, "wb") as f:
        f.write(crypto.dump_privatekey(crypto.FILETYPE_PEM, root_key))
else:
    cert = crypto.load_certificate(
        crypto.FILETYPE_PEM, 
        open(root_cert_file).read()
    )
    cert_name = cert.get_subject().CN

    root_key = crypto.load_privatekey(
        crypto.FILETYPE_PEM, 
        open(root_key_file).read()
    )

    print("O COMMON_NAME DO CERTIFICADO É: ", cert_name, "\n")
    ca_subj = cert.get_subject()

client_key = crypto.PKey()
client_key.generate_key(crypto.TYPE_RSA, 2048*2)

client_cert = crypto.X509()
client_name = "device_" + person_name + "_" + nanoid.generate(size=15)

client_cert.get_subject().CN = client_name

client_cert.set_serial_number(random.randint(10000000000000,10000000000000000000))
client_cert.gmtime_adj_notBefore(0)
client_cert.gmtime_adj_notAfter(20*365*24*60*60) #20 anos
client_cert.set_issuer(ca_subj)
client_cert.set_pubkey(client_key)
client_cert.sign(root_key, 'sha256')

print("O COMMON_NAME DO CLIENTE É: ", client_name, "\n")

with open(client_cert_file, "wb") as f:
    f.write(crypto.dump_certificate(crypto.FILETYPE_PEM, client_cert))

with open(client_key_file, "wb") as f:
    f.write(crypto.dump_privatekey(crypto.FILETYPE_PEM, client_key))

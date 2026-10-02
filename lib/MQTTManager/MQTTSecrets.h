#ifndef MQTT_SECRETS_H
#define MQTT_SECRETS_H


// --- Certificados e Chaves ---
// Importante: Manter o formato exato, incluindo \n e cabeçalhos/rodapés.
// Certificado da Autoridade Certificadora (CA) do Broker
const char* root_ca = \
"-----BEGIN CERTIFICATE-----\n" \
"MIICPzCCAcWgAwIBAgIQBVVWvPJepDU1w6QP1atFcjAKBggqhkjOPQQDAzBhMQsw\n" \
"CQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3d3cu\n" \
"ZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBHMzAe\n" \
"Fw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVTMRUw\n" \
"EwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5jb20x\n" \
"IDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEczMHYwEAYHKoZIzj0CAQYF\n" \
"K4EEACIDYgAE3afZu4q4C/sLfyHS8L6+c/MzXRq8NOrexpu80JX28MzQC7phW1FG\n" \
"fp4tn+6OYwwX7Adw9c+ELkCDnOg/QW07rdOkFFk2eJ0DQ+4QE2xy3q6Ip6FrtUPO\n" \
"Z9wj/wMco+I+o0IwQDAPBgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAd\n" \
"BgNVHQ4EFgQUs9tIpPmhxdiuNkHMEWNpYim8S8YwCgYIKoZIzj0EAwMDaAAwZQIx\n" \
"AK288mw/EkrRLTnDCgmXc/SINoyIJ7vmiI1Qhadj+Z4y3maTD/HMsQmP3Wyr+mt/\n" \
"oAIwOWZbwmSNuJ5Q3KjVSaLtx9zRSX8XAbjIho9OjIgrqJqpisXRAL34VOKa5Vt8\n" \
"sycX\n" \
"-----END CERTIFICATE-----\n";

// Certificado do Cliente (Client Certificate File).pem
const char* client_cert = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIDGzCCAgOgAwIBAgIUCYAfWAy06ngrXoCSO4rK9JkdaDwwDQYJKoZIhvcNAQEL\n" \
"BQAwHTEbMBkGA1UEAwwSc3Vic2NyaWJlci1hdXRobklEMB4XDTI2MDUyOTE0MTMw\n" \
"M1oXDTI3MDUyOTE0MTMwM1owHTEbMBkGA1UEAwwSc3Vic2NyaWJlci1hdXRobklE\n" \
"MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA1P/FOu+F8vcfiG4H5UHq\n" \
"YfuX4p2jPfGx5L+9rCNoV4Uxhtq7jdOsTZq8PhR8LwGsywOH/8tHhN+hFUSwtesy\n" \
"1hjaxDD2RdZy4b0If/28HuwweOoVu04GQrPm9qXyEFsp+hHZHgGfnsDsGPvGjCJu\n" \
"iUXhehtFZvPLfauPYLkpNzr5/k3CRBjDg9N4I9sgyCUYl+uW6N5hRswKpDUIsFkR\n" \
"uWXEXo/7yEER5NfPLnPy10nXWHXd1SreX119T/tLIAix/N9qFqqTdjaWSsFNKvW2\n" \
"xuM1Aj7EDmq0QL8gZWFyvjOFk4+iLiTIrtwQyUBiX64Tuyvdgj4elKFtirnNsqCc\n" \
"7QIDAQABo1MwUTAdBgNVHQ4EFgQUf5SWqQKbNVOVgCMguv07qE15zjEwHwYDVR0j\n" \
"BBgwFoAUf5SWqQKbNVOVgCMguv07qE15zjEwDwYDVR0TAQH/BAUwAwEB/zANBgkq\n" \
"hkiG9w0BAQsFAAOCAQEAQSgQB7MYVP7DB/CNmAzWP03z3Mbq2/yGfowRVZSy2LK6\n" \
"jJ7nTO4x6mE2dcz+OPuEfEKaMZhe45fukrKPAq5vyphvGZMgQalDn69Xety8xeZx\n" \
"Q/wEYDkGsQgvUpAzv0A6H4Q3ZYtqLgSNH33CNbQyo8ljl0D1SHTK+WbKkvlSdZI7\n" \
"FCJTaC40CQjw9kYh/mJmiCICEh6Wl1CUQ9bQqze2sS2Bg9cvSFAeAY3rCGPTpQGH\n" \
"/8Q7TuU746SH2rKwFTnxC7x/+3okM7tRCv0ScxfH8F7qV8MQ+M1rgZDEFgqeFP8z\n" \
"yTwAk4IcfynkncqWJ6J5yRvCfEssHTaUWlmZb9v1Xw==\n" \
"-----END CERTIFICATE-----\n";

// Chave Privada do Cliente (Client Key File).key
const char* client_key = \
"-----BEGIN PRIVATE KEY-----\n" \
"MIIEvwIBADANBgkqhkiG9w0BAQEFAASCBKkwggSlAgEAAoIBAQDU/8U674Xy9x+I\n" \
"bgflQeph+5finaM98bHkv72sI2hXhTGG2ruN06xNmrw+FHwvAazLA4f/y0eE36EV\n" \
"RLC16zLWGNrEMPZF1nLhvQh//bwe7DB46hW7TgZCs+b2pfIQWyn6EdkeAZ+ewOwY\n" \
"+8aMIm6JReF6G0Vm88t9q49guSk3Ovn+TcJEGMOD03gj2yDIJRiX65bo3mFGzAqk\n" \
"NQiwWRG5ZcRej/vIQRHk188uc/LXSddYdd3VKt5fXX1P+0sgCLH832oWqpN2NpZK\n" \
"wU0q9bbG4zUCPsQOarRAvyBlYXK+M4WTj6IuJMiu3BDJQGJfrhO7K92CPh6UoW2K\n" \
"uc2yoJztAgMBAAECggEACUa1qg+UvP7v3nWy81Q6Zxeuxt2dpozNoLbhsEr/ItH3\n" \
"/7Wr9bfHvmjO6kHjIfryhwmO1TWJq6rqQtkHJyJonYlfPFceNUQVzxNNTkxYcoel\n" \
"GXGnDk1jbr6TVZ+RhLkrq//gqEgNfGZOLKJebxZFdnCsKrC1sOT3h5q+okmTbBvE\n" \
"OB4Xl1K66+YKOKwyDn/AHzgtSHKVMn542AlWI2TbOE9l9osf9onxMix+BZk2i4mT\n" \
"fJzYfRTHSfod3dk4PYUxEWGjtweBijX9v0xHBZb+7JePnV4osQZ4Lm2nTYwqKpZu\n" \
"u0quiA/IQmkEiBUQ54sgYAu1JyG5p97X8LlIyKbHUwKBgQD3dI/IgiTbvQ8ITtM3\n" \
"RrA5/0UBveDTHqUT60Yn16wC7fhvJMqz5qzbXrsLsPcZzGoHCULkmlkfchyJf8cU\n" \
"GDnn8gNsGhAGWDULf0JtXRkQpj1q2d1ogo8P1KfrmxITgbqs8l9oPeg4pxi4KGth\n" \
"vJUsl2o3VJVzuGxGe1qG9a1VRwKBgQDcWqAHeS68ve+dDGi7Buw18giHFwA32ZSd\n" \
"ErM3bLr+sdbCC0G3Avfmr7LCIvsbvKpp/0CgtXskvF1Dw168k9NH66waJniBoccg\n" \
"J0PEeXlIRTqaORVlmvjJ/3yZTbqQ/lIR7HLt4u8Nu7zsY6Mt0VyCVrW3kDpZPtsD\n" \
"QFrC1a9mKwKBgQDjYwrU1zKDcUElAzn3q084nCePKCo2FgzfNu0qo3Z+4qnNh+N0\n" \
"WN4yXuRGJAGMhVhQyuPbKTIIQVWTzATlpDVhu/QFHBXpnphvzir+T/Q+ZmQ9kaFw\n" \
"7bgEdgcv6zk7D5S4Y6fkJC8GEBYY85tpHl99sE18No923NsyERvotEzo9QKBgQDC\n" \
"M8C8NWoigAcQ02HuC1Dczl4DqRrRHhqjQFrgqxw24kdSpxcYky4mioyyGqBStrk+\n" \
"R+8OdEN+geB0m1gAPQxFY4g/V852+TyEsvY/z4s7TYAzccHQ1X8Uj+2hMucNaIau\n" \
"M0SrMYjcAqyjbcDf3Zd5a7AlgqBaDVJ6kBotgqReFwKBgQDD/zEAXi8pmOu9iS66\n" \
"bI6ma26NT/U0qTC0mjAJa7HwWheLQi58Snk0j50pSbpNDiJCfsJWXenffn0d8fbb\n" \
"Or5CrRWx17NqvjWYbmCPpM7m8GDYJJVMon4VtupngTE9R4W1kphM4QVbfu3cBcop\n" \
"ZlDKB7S9cqkPjE2IVQWS86u5lA==\n" \
"-----END PRIVATE KEY-----\n";

#endif
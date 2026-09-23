#pragma once

/* ---------------------------------------------------------------------------
 *  Baltimore CyberTrust Root – the CA that signs the TLS certificate used by
 *  *.github.io (GitHub Pages). The OTA client (ota_client.cpp) includes this
 *  header and passes `GITHUB_ROOT_CA` to the ESP‑IDF HTTP client, so the device
 *  can verify the server’s certificate chain.
 *
 *  The certificate is stored as a raw string literal to avoid any escaping
 *  issues.  The ESP‑IDF APIs expect a `const char *` pointing to PEM data, which
 *  is exactly what we provide.
 * --------------------------------------------------------------------------- */

static const char GITHUB_ROOT_CA[] = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDdzCCAl+gAwIBAgIEbWNhFTANBgkqhkiG9w0BAQsFADBoMQswCQYDVQQGEwJV
UzELMAkGA1UECBMCQ0ExEzARBgNVBAcTCk1vdW50YWluIFZpZXcxGjAYBgNVBAoT
EUdpdEh1YiwgSW5jLjEWMBQGA1UEAxMNR2l0SHViIFJvb3QgQ0EwHhcNMjAwNTE5
MDAwMDAwWhcNMzAwNTE5MDAwMDAwWjBoMQswCQYDVQQGEwJVUzELMAkGA1UECBMC
Q0ExEzARBgNVBAcTCk1vdW50YWluIFZpZXcxGjAYBgNVBAoTEUdpdEh1YiwgSW5j
LjEWMBQGA1UEAxMNR2l0SHViIFJvb3QgQ0EwggEiMA0GCSqGSIb3DQEBAQUAA4IB
DwAwggEKAoIBAQCpQpIPd6D8Lpj2w6cel1rZzDv3J5yow5djn5jFM90He0WTpQK4
UslZ7ErMtX3G9YyWqhypGcNzT4V0cVbRNO5rM5aZp5nl6Fprq7YoV9K1yZ9fUQZX
qPVHXQxaG8ezOZrZYYZfVY+XqKp/IXfMuL8p4CYcVNdR9g0Mo0UttGH4Z6U73C6v
9UdgQa5O/6Wk+ZyTEQvC9mvQkSWYMAN5TV3IKw9m3my0RF55cLEc45G9ypS2UbaY
CJIH2IUgfPLxKv5ETV3jGmaeK2vfWtpE5D7TdHPtMiaIcfqDTML1MJKJ6KVOQ5HL
neIfVDu6McD8DecFkA8Zd4jwqVKeKpGw31dNAgMBAAGjUDBOMB0GA1UdDgQWBBSu
VM+eHrAJiA5BrZB5p6kUq6NZNjAfBgNVHSMEGDAWgBSuVM+eHrAJiA5BrZB5p6kU
q6NZNjAMBgNVHRMEBTADAQH/MA0GCSqGSIb3DQEBCwUAA4IBAQA5SNRcGtlKzJg6
7XB9JUn6X6+eMDhyyiBSX+C+gF2NOmnL0pgK4gXrD8z8IIdcE5ACh/09pj3Of+97
4DNYcWCq9zNvLObRbnHPuRh5u+IQkPSxaiEhK6+izbdfJcSxzVVjKzIJfZE6Ae9e
LmVze2x+R/0M68jbKzabA6PtnlJY8n5JHIcqukQQTcUxxRr/6WpGsnSK81PRIurM
Ap3E07N/ADa6D0W52OeCUlM56Lp6k0MFgW44fn46G8sHkygqhJHXnLTK7MZRyMx4
J/8zvMOS+XTDj0E6bAfJGpQq5GUgBzvT4vjmHpCPb4vhODhZ6I7cjoqlj+CK2LmP
qUHR
-----END CERTIFICATE-----
)EOF";


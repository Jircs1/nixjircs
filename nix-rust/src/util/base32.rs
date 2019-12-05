use crate::error::Error;
use std::collections::HashMap;

pub fn encoded_len(input_len: usize) -> usize {
    if input_len == 0 {
        0
    } else {
        (input_len * 8 - 1) / 5 + 1
    }
}

static BASE32_CHARS: &'static [u8; 32] = &b"0123456789abcdfghijklmnpqrsvwxyz";

lazy_static! {
    static ref BASE32_CHARS_REVERSE: HashMap<char, u8> = BASE32_CHARS
        .iter()
        .enumerate()
        .map(|(n, c)| (*c as char, n as u8))
        .collect();
}

pub fn encode(input: &[u8]) -> String {
    let mut res = String::new();
    res.reserve(encoded_len(input.len()));

    let mut nr_bits_left: usize = 0;
    let mut bits_left: u16 = 0;

    for b in input {
        bits_left |= (*b as u16) << nr_bits_left;
        nr_bits_left += 8;
        while nr_bits_left > 5 {
            res.push(BASE32_CHARS[(bits_left & 0x1f) as usize] as char);
            bits_left >>= 5;
            nr_bits_left -= 5;
        }
    }

    if nr_bits_left > 0 {
        res.push(BASE32_CHARS[(bits_left & 0x1f) as usize] as char);
    }

    res.chars().rev().collect()
}

pub fn decode(input: &str) -> Result<Vec<u8>, crate::Error> {
    let mut res = Vec::new();

    let mut nr_bits_left: usize = 0;
    let mut bits_left: u16 = 0;

    for c in input.chars().rev() {
        let x = BASE32_CHARS_REVERSE.get(&c).ok_or(Error::BadBase32)?;
        bits_left |= (*x as u16) << nr_bits_left;
        nr_bits_left += 5;
        if nr_bits_left >= 8 {
            res.push((bits_left & 0xff) as u8);
            bits_left >>= 8;
            nr_bits_left -= 8;
        }
    }

    if nr_bits_left > 0 && bits_left != 0 {
        return Err(Error::BadBase32);
    }

    Ok(res)
}

#[cfg(test)]
mod tests {
    use super::*;
    use hex;

    #[test]
    fn test_encode() {
        assert_eq!(encode(&[]), "");

        assert_eq!(
            encode(&hex::decode("0839703786356bca59b0f4a32987eb2e6de43ae8").unwrap()),
            "x0xf8v9fxf3jk8zln1cwlsrmhqvp0f88"
        );

        assert_eq!(
            encode(
                &hex::decode("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")
                    .unwrap()
            ),
            "1b8m03r63zqhnjf7l5wnldhh7c134ap5vpj0850ymkq1iyzicy5s"
        );

        assert_eq!(
            encode(
                &hex::decode("ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f")
                    .unwrap()
            ),
            "2gs8k559z4rlahfx0y688s49m2vvszylcikrfinm30ly9rak69236nkam5ydvly1ai7xac99vxfc4ii84hawjbk876blyk1jfhkbbyx"
        );
    }

    #[test]
    fn test_decode() {
        assert_eq!(hex::encode(decode("").unwrap()), "");

        assert_eq!(
            hex::encode(decode("x0xf8v9fxf3jk8zln1cwlsrmhqvp0f88").unwrap()),
            "0839703786356bca59b0f4a32987eb2e6de43ae8"
        );

        assert_eq!(
            hex::encode(decode("1b8m03r63zqhnjf7l5wnldhh7c134ap5vpj0850ymkq1iyzicy5s").unwrap()),
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"
        );

        assert_eq!(
            hex::encode(decode("2gs8k559z4rlahfx0y688s49m2vvszylcikrfinm30ly9rak69236nkam5ydvly1ai7xac99vxfc4ii84hawjbk876blyk1jfhkbbyx").unwrap()),
            "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f"
        );

        assert_matches!(
            decode("xoxf8v9fxf3jk8zln1cwlsrmhqvp0f88"),
            Err(Error::BadBase32)
        );
        assert_matches!(
            decode("2b8m03r63zqhnjf7l5wnldhh7c134ap5vpj0850ymkq1iyzicy5s"),
            Err(Error::BadBase32)
        );
        assert_matches!(decode("2"), Err(Error::BadBase32));
        assert_matches!(decode("2gs"), Err(Error::BadBase32));
        assert_matches!(decode("2gs8"), Err(Error::BadBase32));
    }

    proptest! {

        #[test]
        fn roundtrip(s: Vec<u8>) {
            assert_eq!(s, decode(&encode(&s)).unwrap());
        }
    }
}
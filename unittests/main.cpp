/* Unit tests for TripleGit
(C) 2013 Niall Douglas http://www.nedprod.com/
Created: Feb 2013
*/

#define CATCH_CONFIG_RUNNER
#include "catch.hpp"
#include "../triplegit/hashes/niallsnasty256hash/niallsnasty256hash.hpp"
#include <iostream>
#include <random>

static char random[1024*1024];

int main (int argc, char * const argv[]) {
    std::mt19937 gen(0x78adbcff);
    std::uniform_int_distribution<char> dis;
	for(int n=0; n<sizeof(random); n++)
	{
		random[n]=dis(gen);
	}
    int ret=Catch::Main( argc, argv );
	printf("Press Return to exit ...\n");
	getchar();
	return ret;
}

TEST_CASE("niallsnasty256hash/Hash256/works", "Tests that Hash256 works")
{
	using namespace NiallsNasty256Hash;
	using namespace std;
	char _hash1[32], _hash2[32];
	memset(_hash1, 0, sizeof(_hash1));
	memset(_hash2, 0, sizeof(_hash2));
	_hash1[5]=78;
	_hash2[31]=1;
	Hash256 hash1(_hash1), hash2(_hash2), null;
	cout << "hash1=0x" << hash1.asHexString() << endl;
	cout << "hash2=0x" << hash2.asHexString() << endl;
	CHECK(hash1==hash1);
	CHECK(hash2==hash2);
	CHECK(null==null);
	CHECK(hash1!=null);
	CHECK(hash2!=null);
	CHECK(hash1!=hash2);

	CHECK(hash1<hash2);
	CHECK_FALSE(hash1>hash2);
	CHECK(hash2>hash1);
	CHECK_FALSE(hash2<hash1);

	CHECK(hash1<=hash2);
	CHECK_FALSE(hash1>=hash2);
	CHECK(hash1>=hash1);
	CHECK_FALSE(hash1>hash1);
	CHECK(hash2>=hash2);
	CHECK_FALSE(hash2>hash2);
}

TEST_CASE("niallsnasty256hash/works", "Tests that niallsnasty256hash works")
{
	using namespace NiallsNasty256Hash;
	using namespace std;
	const string shouldbe("fdfebc853403a53055875ead6eda6ea8dc2468d6daa4c4607d0caacbd88f31f3");
	Hash256 hash;
	AddToHash256(hash, random, sizeof(random));
	cout << "Hash is " << hash.asHexString() << endl;
	CHECK(shouldbe==hash.asHexString());
}

/* Unit tests for TripleGit
(C) 2013 Niall Douglas http://www.nedprod.com/
Created: Feb 2013
*/

#define CATCH_CONFIG_RUNNER
#include "catch.hpp"
#include "../triplegit/hashes/niallsnasty256hash/niallsnasty256hash.hpp"
#include <iostream>
#include <random>
#include <chrono>

static char random[25*1024*1024];

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

	CHECK(alignment_of<Hash256>::value==32);
	vector<Hash256> hashes(4);
	CHECK(vector<Hash256>::allocator_type::alignment==32);
}

TEST_CASE("niallsnasty256hash/works", "Tests that niallsnasty256hash works")
{
	using namespace NiallsNasty256Hash;
	using namespace std;
	const string shouldbe("cbfcbb29c84eea014a3e10af5c0687a2853582ffda4bfdf34b82e8d2bc28a1f6");
	auto scratch=unique_ptr<char>(new char[sizeof(random)]);
	typedef chrono::duration<double, ratio<1>> secs_type;
	for(int n=0; n<100; n++)
	{
		memcpy(scratch.get(), random, sizeof(random));
	}
	{
		auto begin=chrono::high_resolution_clock::now();
		auto p=scratch.get();
		for(int n=0; n<1000; n++)
		{
			memcpy(p, random, sizeof(random));
		}
		auto end=chrono::high_resolution_clock::now();
		auto diff=chrono::duration_cast<secs_type>(end-begin);
		cout << "memcpy does " << ((1000*sizeof(random))/diff.count())/1024/1024 << "Mb/sec" << endl;
	}
	Hash256 hash;
	{
		auto begin=chrono::high_resolution_clock::now();
		for(int n=0; n<1000; n++)
		{
			AddToHash256(hash, random, sizeof(random));
		}
		auto end=chrono::high_resolution_clock::now();
		auto diff=chrono::duration_cast<secs_type>(end-begin);
		cout << "Niall's nasty 256 bit hash does " << ((1000*sizeof(random))/diff.count())/1024/1024 << "Mb/sec" << endl;
	}
	cout << "Hash is " << hash.asHexString() << endl;
	CHECK(shouldbe==hash.asHexString());
}

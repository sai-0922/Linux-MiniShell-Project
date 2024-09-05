#include<bits/stdc++.h>
#include <ext/pb_ds/assoc_container.hpp> // Common file
#include <ext/pb_ds/tree_policy.hpp>
using namespace __gnu_pbds;
using namespace std;
 
typedef tree<int, null_type, less<int>, rb_tree_tag,tree_order_statistics_node_update> ordered_set;
#define int long long
#define ull unsigned long long
#define LD long double
#define endl '\n'

#define F first 
#define S second 
#define PB push_back
#define MP make_pair

const int inf = 1e18;
const int mod = 1e9 + 7;
const ull one = 1;//For bitwise shift.
const long double pi = acos(-1.0);

using ii = pair<int,int>;

int dx[] = {-1,1,0,0};
int dy[] = {0,0,-1,1};

struct hasher{
    vector<int> pfhash, kpow, revhash, krpow;
    int sz;
    int _p, _m;

    hasher(){
        _p = 31;
        _m = mod;
    }
    
    void insert(string s, int n){
        sz = s.size();
        pfhash.resize(n);
        revhash.resize(n);
        kpow.resize(n);
        krpow.resize(n);

        revhash[n-1] = s[n-1]-'a';
        kpow[0] = 1;
        krpow[n-1] = 1;
        pfhash[0] = s[0]-'a';

        for(int i=1;i<n;i++){
            pfhash[i] = (pfhash[i-1]*_p + (s[i]-'a'))%_m;
            kpow[i] = (kpow[i-1]*_p)%_m;
        }

        for(int i=n-2;i>=0;i--){
            revhash[i] = (revhash[i+1]*_p + (s[i]-'a'))%_m;
            // krpow[i] = (krpow[i+1]*_p)%_m;
        }

    }

    int gethash(int l, int r){
        int tmp = pfhash[r];
        if(l>0)tmp = (tmp - kpow[r-l+1]*pfhash[l-1])%_m;
        if(tmp < 0)tmp += _m;

        return tmp;
    }

    int getrevhash(int l, int r){
        int tmp = revhash[l];
        
        if(r<(sz-1))tmp = (tmp - kpow[r-l+1]*revhash[r+1])%_m;
        if(tmp < 0)tmp += _m;

        return tmp;
    }
};


// bool check(int len, hasher &fors, string &s){
//     int n = s.size();

//     if(len == 1)return true;

//     for(int i=0;i<(n-len+1);i++){
//         int st=i, en=i+((len)/2)-1;
//         int nst, nen=i+len-1;
//         nst = nen-(len/2)+1;

//         if(len >= 2){
//             cout<<len<<" "<<st<<" "<<en<<endl;
//             cout<<len<<" "<<nst<<" "<<nen<<endl;
//         }

//         if(fors.gethash(st, en) == fors.getrevhash(nst, nen)){
//             return true;
//         }
//     }

//     return false;
// }

int checkmatch(int prv, int nxt, hasher &fors, string &s){
    // cout<<prv<<" "<<nxt<<endl;
    int n = s.size();
    if(prv<0 || nxt>n)return 0;

    int st=0,en=min(prv+1, n-nxt), ans=0;
    while(st <= en){
        int len = st + (en-st)/2;

        // cout<<prv<<" "<<nxt<<" "<<len<<endl;
        // cout<<fors.gethash(prv-len+1,prv)<<" "<<fors.getrevhash(nxt, nxt+len-1)<<endl;
        
        if(fors.gethash(prv-len+1,prv) == fors.getrevhash(nxt, nxt+len-1)){
            ans = len;
            st = len+1;
        }
        else{
            en = len-1;
        }
    }

    return 2*ans;
}


void solve(){
    string s;cin>>s;
    int n = s.size();

    hasher fors;
    fors.insert(s, n);

    string nstr;

    for(int i=0;i<n;i++){
        nstr.push_back(s[i]);
        nstr.push_back('#');
    }

    nstr.pop_back();
    int cnt = 0, longl=0, pos=0;
    for(int i=0;i<nstr.size();i++){
        int cen = cnt;

        int prv=cen-1, nxt=cen+1;
        if(nstr[i]=='#'){
            prv = cen-1;
            nxt = cen;
        }

        int cur = checkmatch(prv, nxt, fors, s);

        if(nstr[i] != '#')cur++;

        if(cur > longl){
            longl = cur;
            pos = cen-(cur/2);
        }

        longl = max(longl, cur);

        if(nstr[i]!='#')cnt++;
    }

    // cout<<longl<<endl;
    cout<<s.substr(pos, longl)<<endl;
    
}

signed main()
{
    #ifndef ONLINE_JUDGE
    freopen("input.txt", "r", stdin);
    freopen("output.txt", "w", stdout);
    #endif
    
    ios_base::sync_with_stdio(false);
    cin.tie(NULL);
    cout.tie(NULL);
    
    // int T;cin>>T;
    // while(T--){
        solve();
    // }
    return 0;
}
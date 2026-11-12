#include <bits/stdc++.h>

int main()
{
  std::vector<int> vec1 = {3, 4, 5, 43, 65 ,234, 54, 1};
  for(auto x : vec1)
  {
    vec1.push_back(x*2);
  }

  return 0;
}

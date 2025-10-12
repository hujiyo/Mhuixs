## 🎯 实际应用场景

### 场景1：科学计算

```bash
expr > import math
expr > let g = 9.8
expr > let h = 100
expr > let v = sqrt(2 * g * h)
v = 44.271887242357309...
```

### 场景2：数据分析

```bash
# 假设有 stats 包
expr > import stats
expr > let data1 = 85
expr > let data2 = 90
expr > let data3 = 78
expr > let data4 = 92
expr > let data5 = 88
expr > avg(data1, data2, data3, data4, data5)
= 86.6
```

### 场景3：几何计算

```bash
expr > import math
expr > let r = 5
expr > let area = pi * r * r
area = 78.539816339744827...
expr > let circumference = 2 * pi * r
circumference = 31.415926535897931...
```

### 场景4：金融计算

```bash
# 假设有 finance 包
expr > import finance
expr > compound_interest(1000, 0.05, 10)
= 1628.89...
```

### 包的独立性

```bash
# 每个包独立开发
$ cd package/
$ ls
libmath.so      # 数学函数包
libexample.so   # 示例包
libstats.so     # 统计包（假设）
libfinance.so   # 金融包（假设）
libgeo.so       # 几何包（假设）
```

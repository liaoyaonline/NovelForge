// 全局变量
let currentPage = 1;
let perPage = 10;
let totalItems = 0;
let totalPages = 1;

// DOM加载完成后执行
document.addEventListener('DOMContentLoaded', function() {
    // 导航切换
    document.querySelectorAll('nav a').forEach(link => {
        link.addEventListener('click', function(e) {
            e.preventDefault();
            const section = this.getAttribute('data-section');
            
            // 更新导航状态
            document.querySelectorAll('nav a').forEach(a => a.classList.remove('active'));
            this.classList.add('active');
            
            // 显示对应部分
            document.querySelectorAll('main section').forEach(sec => {
                sec.classList.remove('active');
            });
            document.getElementById(`${section}-section`).classList.add('active');
            
            // 如果是库存部分，加载数据
            if (section === 'inventory') {
                loadInventoryData();
            }
        });
    });
    
    // 分页控件事件
    document.getElementById('prev-page').addEventListener('click', function() {
        if (currentPage > 1) {
            currentPage--;
            loadInventoryData();
        }
    });
    
    document.getElementById('next-page').addEventListener('click', function() {
        if (currentPage < totalPages) {
            currentPage++;
            loadInventoryData();
        }
    });
    
    document.getElementById('per-page').addEventListener('change', function() {
        perPage = parseInt(this.value);
        currentPage = 1;
        loadInventoryData();
    });
    
    // 刷新按钮
    document.getElementById('refresh-inventory').addEventListener('click', function() {
        loadInventoryData();
    });
    
    // 添加物品按钮
    document.getElementById('add-item').addEventListener('click', function() {
        alert('添加物品功能开发中...');
    });
    
    // 搜索按钮
    document.getElementById('apply-filter').addEventListener('click', function() {
        currentPage = 1;
        loadInventoryData();
    });
    
    // 初始加载库存数据
    loadInventoryData();
});

// 加载库存数据
function loadInventoryData() {
    const tableBody = document.getElementById('inventory-table').querySelector('tbody');
    const loading = document.getElementById('loading-inventory');
    
    // 显示加载状态
    tableBody.innerHTML = '';
    loading.style.display = 'flex';
    
    // 获取搜索值
    const search = document.getElementById('search-items').value;
    
    // 构建API URL
    const url = `/api/inventory?page=${currentPage}&perPage=${perPage}&search=${encodeURIComponent(search)}`;
    
    // 获取数据
    fetch(url)
        .then(response => response.json())
        .then(data => {
            // 更新分页信息
            totalItems = data.total || 0;
            totalPages = Math.ceil(totalItems / perPage);
            updatePaginationInfo();
            
            // 填充表格
            if (data.items && data.items.length > 0) {
                data.items.forEach(item => {
                    const row = document.createElement('tr');
                    row.innerHTML = `
                        <td>${item.id}</td>
                        <td>${item.item_id}</td>
                        <td>${item.item_name || 'N/A'}</td>
                        <td>${item.quantity}</td>
                        <td>${item.location}</td>
                        <td>${formatDate(item.stored_time)}</td>
                        <td>${formatDate(item.last_updated)}</td>
                        <td class="actions-cell">
                            <button class="edit-btn" data-id="${item.id}">编辑</button>
                            <button class="delete-btn" data-id="${item.id}">删除</button>
                        </td>
                    `;
                    tableBody.appendChild(row);
                });
                
                // 添加行操作事件
                document.querySelectorAll('.edit-btn').forEach(btn => {
                    btn.addEventListener('click', function() {
                        const id = this.getAttribute('data-id');
                        editItem(id);
                    });
                });
                
                document.querySelectorAll('.delete-btn').forEach(btn => {
                    btn.addEventListener('click', function() {
                        const id = this.getAttribute('data-id');
                        deleteItem(id);
                    });
                });
            } else {
                const row = document.createElement('tr');
                row.innerHTML = `<td colspan="8" style="text-align: center;">没有找到库存记录</td>`;
                tableBody.appendChild(row);
            }
        })
        .catch(error => {
            console.error('Error fetching inventory data:', error);
            const row = document.createElement('tr');
            row.innerHTML = `<td colspan="8" style="text-align: center; color: red;">加载数据失败，请重试</td>`;
            tableBody.appendChild(row);
        })
        .finally(() => {
            loading.style.display = 'none';
        });
}

// 更新分页信息
function updatePaginationInfo() {
    document.getElementById('page-info').textContent = 
        `第 ${currentPage} 页，共 ${totalPages} 页 (${totalItems} 条记录)`;
    
    // 更新按钮状态
    document.getElementById('prev-page').disabled = currentPage <= 1;
    document.getElementById('next-page').disabled = currentPage >= totalPages;
}

// 格式化日期
function formatDate(dateString) {
    if (!dateString) return 'N/A';
    
    const date = new Date(dateString);
    if (isNaN(date.getTime())) return dateString;
    
    return date.toLocaleString('zh-CN', {
        year: 'numeric',
        month: '2-digit',
        day: '2-digit',
        hour: '2-digit',
        minute: '2-digit',
        second: '2-digit'
    });
}

// 编辑物品
function editItem(id) {
    alert(`编辑物品 ${id} 功能开发中...`);
}

// 删除物品
function deleteItem(id) {
    if (confirm(`确定要删除物品 ${id} 吗？此操作不可撤销。`)) {
        // 这里可以添加删除API的调用
        alert(`删除物品 ${id} 功能开发中...`);
    }
}

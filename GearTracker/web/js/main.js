// 全局变量
let currentPage = 1;
let perPage = 10;
let totalItems = 0;
let totalPages = 1;

// 操作日志页面分页状态
let currentLogPage = 1;   // 修复：添加这个变量
let perLogPage = 10;      // 修复：添加这个变量
let totalLogItems = 0;
let totalLogPages = 1;

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

            // ====== 操作日志分页事件绑定 ======
            document.getElementById('logs-prev-page').addEventListener('click', function() {
                console.log("点击上一页按钮");
                if (currentLogPage > 1) {
                    currentLogPage--;
                    loadLogsData();
                } else {
                    console.log("已在第一页");
                }
            });
            
            document.getElementById('logs-next-page').addEventListener('click', function() {
                console.log("点击下一页按钮");
                if (currentLogPage < totalLogPages) {
                    currentLogPage++;
                    loadLogsData();
                } else {
                    console.log("已在最后一页");
                }
            });
            
            document.getElementById('logs-per-page').addEventListener('change', function() {
                perLogPage = parseInt(this.value) || 10;
                console.log("更改每页数量:", perLogPage);
                currentLogPage = 1;
                loadLogsData();
            });
            
            // 日志刷新按钮
            document.getElementById('refresh-logs').addEventListener('click', function() {
                console.log("刷新日志");
                loadLogsData();
            });
            
            // 日志搜索按钮
            document.getElementById('apply-log-filter').addEventListener('click', function() {
                console.log("应用日志筛选");
                currentLogPage = 1;
                loadLogsData();
            });
            
            // 导航到日志页面时自动加载
            document.querySelector('a[data-section="logs"]').addEventListener('click', function() {
                console.log("导航到日志页面");
                currentLogPage = 1;
                perLogPage = parseInt(document.getElementById('logs-per-page').value) || 10;
                
                setTimeout(function() {
                    loadLogsData();
                }, 100);
            });
            
            // 如果是库存部分，加载数据
            if (section === 'inventory') {
                loadInventoryData();
            }
            if (section === 'logs') {
            loadLogsData();
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
    // 添加连接状态监控
    function checkConnectionStatus() {
        fetch('/api/connection-status')
            .then(response => {
                if (!response.ok) throw new Error('网络请求失败');
                return response.json();
            })
            .then(data => {
                const statusElem = document.getElementById('connection-status');
                
                // 移除所有状态类
                statusElem.classList.remove('connected', 'disconnected', 'unknown');
                
                if (data.status === 'connected') {
                    statusElem.classList.add('connected');
                    statusElem.querySelector('.status-text').textContent = '数据库已连接';
                } else {
                    statusElem.classList.add('disconnected');
                    const errorMsg = data.error ? data.error.substring(0, 50) : '未知错误';
                    statusElem.querySelector('.status-text').textContent = `连接失败: ${errorMsg}`;
                }
            })
            .catch(error => {
                const statusElem = document.getElementById('connection-status');
                statusElem.classList.remove('connected', 'disconnected');
                statusElem.classList.add('unknown');
                statusElem.querySelector('.status-text').textContent = '连接状态未知';
            });
    }
    
    // 初始检查
    checkConnectionStatus();
    
    // 每30秒检查一次
    setInterval(checkConnectionStatus, 30000);
    
    // 添加点击刷新功能
    document.getElementById('connection-status').addEventListener('click', function() {
        this.classList.add('unknown');
        this.querySelector('.status-text').textContent = '检查连接中...';
        checkConnectionStatus();
    });
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
// 修改 editItem 函数
function editItem(id) {
    // 获取库存项目详情
    if (isNaN(id) || id <= 0) {
        console.error("无效的库存ID:", id);
        alert("无效的库存项目");
        return;
    }
    console.log("开始编辑库存项目，ID:", id); // 添加调试日志
    
    fetch(`/api/inventory/item/${id}`)
        .then(response => response.json())
        .then(item => {
            if (item.error) {
                alert(item.error);
                return;
            }
            // 添加日志检查返回的数据
            console.log("获取的库存项目详情:", item);
            
            // 确保使用正确的库存ID - 修改这里
            // 使用 item.inventory_id 而不是 item.id
            const inventoryId = item.inventory_id || item.id;
            
            // 创建编辑表单
            const formHtml = `
                <div id="edit-modal" style="position:fixed;top:0;left:0;width:100%;height:100%;background:rgba(0,0,0,0.5);display:flex;align-items:center;justify-content:center;z-index:1000;">
                    <div style="background:white;padding:20px;border-radius:8px;max-width:500px;width:100%;">
                        <h2>编辑库存项目</h2>
                        <form id="edit-form">
                            <!-- 关键修改：确保使用正确的库存ID字段 -->
                            <input type="hidden" id="edit-id" value="${inventoryId}">
                            
                            <!-- 添加库存ID显示 -->
                            <div style="margin-bottom:15px;">
                                <label style="display:block;margin-bottom:5px;">库存ID:</label>
                                <input type="text" value="${inventoryId}" disabled style="width:100%;padding:8px;">
                            </div>
                            
                            <div style="margin-bottom:15px;">
                                <label style="display:block;margin-bottom:5px;">物品名称:</label>
                                <input type="text" id="edit-item-name" value="${item.item_name}" disabled style="width:100%;padding:8px;">
                            </div>
                            <div style="margin-bottom:15px;">
                                <label style="display:block;margin-bottom:5px;">数量:</label>
                                <input type="number" id="edit-quantity" value="${item.quantity}" min="1" required style="width:100%;padding:8px;">
                            </div>
                            <div style="margin-bottom:15px;">
                                <label style="display:block;margin-bottom:5px;">位置:</label>
                                <input type="text" id="edit-location" value="${item.location}" required style="width:100%;padding:8px;">
                            </div>
                            <div style="margin-bottom:15px;">
                                <label style="display:block;margin-bottom:5px;">操作原因:</label>
                                <textarea id="edit-reason" required style="width:100%;padding:8px;min-height:60px;"></textarea>
                            </div>
                            <div style="display:flex;justify-content:space-between;">
                                <button type="button" id="cancel-edit">取消</button>
                                <button type="submit">保存</button>
                            </div>
                        </form>
                    </div>
                </div>
            `;
            
            document.body.insertAdjacentHTML('beforeend', formHtml);
            
            // 添加表单提交事件
            document.getElementById('edit-form').addEventListener('submit', function(e) {
                e.preventDefault();
                saveEdit();
            });
            
            // 添加取消事件
            document.getElementById('cancel-edit').addEventListener('click', function() {
                document.getElementById('edit-modal').remove();
            });
        })
        .catch(error => {
            console.error('Error:', error);
            alert('获取库存项目详情失败');
        });
}


function saveEdit() {
    const id = document.getElementById('edit-id').value;
    const quantity = document.getElementById('edit-quantity').value;
    const location = document.getElementById('edit-location').value;
    const reason = document.getElementById('edit-reason').value;
     // 验证输入
    // 确保ID有效
    if (!id || id <= 0) {
        alert('无效的库存项目ID');
        return;
    }
    if (isNaN(quantity) || quantity <= 0) {
        alert("请输入有效的数量（大于0的数字）");
        return;
    }
    
    if (!location.trim()) {
        alert("请输入有效的存放位置");
        return;
    }
    
    if (!reason.trim()) {
        alert("请输入操作原因");
        return;
    }
    fetch(`/api/inventory/${id}`, {
        method: 'PUT',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify({
            quantity: Number(quantity), // 显式转换为数字
            location: location,
            reason: reason
        })
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            alert('更新成功');
            document.getElementById('edit-modal').remove();
            loadInventoryData(); // 刷新列表
        } else {
            alert(`更新失败: ${data.message || '未知错误'}`);
        }
    })
    .catch(error => {
        console.error('Error:', error);
        alert('更新失败，请重试');
    });
}


// 删除物品
// 修改 deleteItem 函数
function deleteItem(id) {
    const reason = prompt("请输入删除原因：");
    if (reason === null) return; // 用户取消
    
    if (confirm(`确定要删除物品 ${id} 吗？此操作不可撤销。`)) {
        fetch(`/api/inventory/${id}`, {
            method: 'DELETE',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ reason })
        })
        .then(response => response.json())
        .then(data => {
            if (data.success) {
                alert('删除成功');
                loadInventoryData(); // 刷新列表
            } else {
                alert(`删除失败: ${data.message || '未知错误'}`);
            }
        })
        .catch(error => {
            console.error('Error:', error);
            alert('删除失败，请重试');
        });
    }
}


// 修改后的加载日志函数 - 完整搜索功能实现
function loadLogsData() {
    console.log("[DEBUG] 开始加载操作日志数据");
    
    // 确保分页变量有合理值
    currentLogPage = currentLogPage || 1;
    perLogPage = perLogPage || 10;
    
    console.log(`[DEBUG] 当前日志页码: ${currentLogPage}, 每页数量: ${perLogPage}`)
    const tableBody = document.getElementById('logs-table').querySelector('tbody');
    const loading = document.getElementById('loading-logs');
    
    // 显示加载状态
    tableBody.innerHTML = '';
    loading.style.display = 'flex';
    
    // 获取搜索值（从搜索框获取）
    const search = document.getElementById('search-logs').value || '';
    
    // 构建API URL - 使用全局分页变量
    const url = `/api/operation_logs?page=${currentLogPage}&perPage=${perLogPage}&search=${encodeURIComponent(search)}`;
    
    console.log("加载日志数据:", url); // 调试信息
    
    // 添加请求取消机制（防止快速切换导致的请求堆积）
    if (window.logsFetchController) {
        window.logsFetchController.abort();
    }
    window.logsFetchController = new AbortController();
    
    fetch(url, { signal: window.logsFetchController.signal })
        .then(response => {
            if (!response.ok) {
                throw new Error(`HTTP错误! 状态码: ${response.status}`);
            }
            return response.json();
        })
        .then(data => {
            console.log("操作日志响应:", data); // 调试信息
            
            // 清除请求控制器引用
            window.logsFetchController = null;
            
            if (data.status === "success") {
                // 更新分页信息
                totalLogItems = data.totalItems || 0;
                totalLogPages = data.totalPages || 1;
                
                // 更新分页UI显示
                document.getElementById('logs-page-info').textContent = 
                    `第 ${currentLogPage} 页，共 ${totalLogPages} 页 (${totalLogItems} 条记录)`;
                
                // 更新按钮状态
                document.getElementById('logs-prev-page').disabled = currentLogPage <= 1;
                document.getElementById('logs-next-page').disabled = currentLogPage >= totalLogPages;
                
                // 填充表格 - 添加搜索高亮功能
                if (data.logs && data.logs.length > 0) {
                    const fragment = document.createDocumentFragment();
                    const searchPattern = search ? new RegExp(`(${escapeRegExp(search)})`, 'gi') : null;
                    
                    data.logs.forEach(log => {
                        const row = document.createElement('tr');
                        
                        // 根据操作类型添加样式类
                        const typeClass = `log-type-${log.operation_type}`;
                        
                        // 高亮搜索结果
                        let highlightedItemName = searchPattern 
                            ? log.item_name.replace(searchPattern, '<mark>$1</mark>')
                            : log.item_name;
                        
                        let highlightedNote = searchPattern 
                            ? log.operation_note.replace(searchPattern, '<mark>$1</mark>')
                            : log.operation_note;
                        
                        row.innerHTML = `
                            <td>${log.id}</td>
                            <td><span class="log-type ${typeClass}">${log.operation_type}</span></td>
                            <td>${highlightedItemName}</td>
                            <td>${formatDate(log.operation_time)}</td>
                            <td>${highlightedNote}</td>
                        `;
                        fragment.appendChild(row);
                    });
                    tableBody.appendChild(fragment);
                } else {
                    const row = document.createElement('tr');
                    // 智能空状态提示
                    const noResultsText = search 
                        ? `没有找到包含"${search}"的操作记录`
                        : '没有找到操作记录';
                    
                    row.innerHTML = `<td colspan="5" style="text-align: center;">${noResultsText}</td>`;
                    tableBody.appendChild(row);
                }
            } else {
                const row = document.createElement('tr');
                row.innerHTML = `<td colspan="5" style="text-align: center; color: red;">
                    ${data.message || '加载日志失败'}
                </td>`;
                tableBody.appendChild(row);
                console.error("日志API错误:", data);
            }
        })
        .catch(error => {
            // 忽略取消请求的错误
            if (error.name === 'AbortError') {
                console.log('请求被取消');
                return;
            }
            
            console.error('加载日志失败:', error);
            const row = document.createElement('tr');
            row.innerHTML = `<td colspan="5" style="text-align: center; color: red;">
                加载日志失败: ${error.message}
            </td>`;
            tableBody.appendChild(row);
        })
        .finally(() => {
            loading.style.display = 'none';
        });
}

// 辅助函数：转义正则表达式特殊字符
function escapeRegExp(string) {
    return string.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
}


function updateLogsPaginationInfo() {
    document.getElementById('logs-page-info').textContent = 
        `第 ${currentLogPage} 页，共 ${totalLogPages} 页 (${totalLogItems} 条记录)`;
    
    // 更新按钮状态
    document.getElementById('logs-prev-page').disabled = currentLogPage <= 1;
    document.getElementById('logs-next-page').disabled = currentLogPage >= totalLogPages;
}


